#include <iostream>
#include <thread>
#include "core/filesender_manager.hpp"

const short PORT = 8888;

int main(){
    std::cout<<"Starting broadcast sender and receiver..."<<std::endl;
    FilesenderManager fm(PORT);
    std::cout<<"Login successful!"<<std::endl;
    std::thread broadcast_thread([&fm]() { fm.run_broadcast(); });
    std::cout<<"Please Enter the target username..."<<std::endl;
    broadcast_thread.join();
    return 0;
}