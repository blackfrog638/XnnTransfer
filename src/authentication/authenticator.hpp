#pragma once

#include "../authentication/profile.hpp"

class Authenticator {
    constexpr static const char* PROFILE_PATH = "profile.txt";
    public:
        Account get_profile();
        bool login();
        void regist();
        void main_handler();    
};