#include <iostream>
#include <thread>
#include "core/broadcast/broadcast_manager.hpp"

const short PORT = 8888;

int main(){
    BroadcastManager bm;

    std::thread sender_thread([&bm]() { bm.broadcast_sender(PORT); });
    std::thread receiver_thread([&bm]() { bm.broadcast_receiver(PORT); });

    std::cin.get();
    bm.stop_flag = true;

    sender_thread.join();
    receiver_thread.join();
    
    return 0;
}