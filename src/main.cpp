#include <iostream>
#include <thread>
#include "core/filesender_manager.hpp"

const short PORT = 8888;

int main(){
    std::cout<<"Starting broadcast sender and receiver..."<<std::endl;
    FilesenderManager fm("blackfrog638","123456",PORT);
    std::thread broadcast_thread([&fm]() { fm.run_broadcast(); });
    std::cout<<"Press Enter to stop broadcast..."<<std::endl;
    broadcast_thread.join();
    return 0;
}