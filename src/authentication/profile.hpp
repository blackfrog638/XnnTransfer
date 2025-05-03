#pragma once

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <string>

class Account {
    public:
        std::string name;
        std::string password;
        std::string ip;
        short port = 0;

        Account() = default;
        Account(std::string name, std::string password) : 
            name(std::move(name)), password(std::move(password)){}

        bool operator==(const Account& other) const {
            return name == other.name && password == other.password && ip == other.ip && port == other.port;
        }
        bool operator!=(const Account& other) const {
            return !(*this == other);
        }

        
        template<class Archive>
        void serialize(Archive& ar, const unsigned int version) {
            ar & name;
            //ar & ip;
            ar & password;  
            //!TODO: 密码加密
            //ar & port;
        }

    private:
        

};