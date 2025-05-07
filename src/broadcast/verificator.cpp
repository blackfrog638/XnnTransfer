#include "verificator.hpp"

#include <iostream>

#include <boost/asio.hpp>

namespace net = boost::asio;

int verificator::send_verification_request(const std::string& target_user) {
    try{
        net::io_context io;
        net::ip::tcp protocol = net::ip::tcp::v4();
        net::ip::tcp::socket socket(io);
        
        boost::system::error_code ec;
        socket.open(protocol, ec);

        if(ec){
            std::cout << "Failed to open the socket! Error code = "
			          << ec.value() << ". Message: " << ec.message();
		    return ec.value();
        }
    }
    catch(std::exception &e){
        std::cerr<<"An error has occurred: "<<e.what()<<std::endl;
    }
    return 0;
}