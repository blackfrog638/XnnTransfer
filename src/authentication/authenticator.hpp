#pragma once

#include <filesystem>
#include <iostream>
#include <fstream>
#include "../authentication/profile.hpp"

class Authenticator {
    constexpr static const char* PROFILE_PATH = "profile.txt";
    public:
        bool login() {
            if(std::filesystem::exists(PROFILE_PATH)){
                Account account = get_profile();
                std::cout<<account.name<<"'s Password: ";

                std::string password;
                std::cin>>password;

                if(password != account.password){
                    std::cout<<"Password is incorrect!"<<std::endl;
                    return false;
                }
                return true;
            }
            else{
                std::cout<<"No profile found, please register first!"<<std::endl;
                regist();
                return false;
            }
        }
        
        void regist(){
            std::cout<<"Please input your name: ";
            std::string name;
            std::cin>>name;

            std::cout<<"Please input your password: ";
            std::string password;
            std::cin>>password;

            Account account(name, password);
            
            std::ofstream ofs(PROFILE_PATH);
            boost::archive::text_oarchive oa(ofs);
            
            oa << account;
        }

        void main_handler(){
            while(!login()){
                login();
            }
        }

        Account get_profile(){
            std::ifstream ifs(PROFILE_PATH);
            boost::archive::text_iarchive ia(ifs);

            Account account;
            ia >> account;
            return account;
        }

    private:
        
};