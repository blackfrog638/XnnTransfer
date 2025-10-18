#include "online_list_display.h"
#include "core/timer/spawn_after_delay.h"
#include <chrono>
#include <fmt/format.h>
#include <iomanip>
#include <iostream>
#include <spdlog/spdlog.h>

namespace cli {

void OnlineListDisplay::start() {
    if (running_) {
        return;
    }
    running_ = true;
    spdlog::info("OnlineListDisplay starting...");
    executor_.spawn(display_loop());
}

void OnlineListDisplay::stop() {
    running_ = false;
}

asio::awaitable<void> OnlineListDisplay::display_loop() {
    while (running_) {
        std::cout << "\033[2J\033[H" << std::flush;
        std::cout << "Users Online:\n";
        print_online_list();

        co_await core::timer::spawn_after_delay([]() -> asio::awaitable<void> { co_return; },
                                                kRefreshIntervalMs);
    }
}

void OnlineListDisplay::print_online_list() {
    auto online_list = inspector_.get_online_list();

    if (online_list.empty()) {
        std::cout << "no online user yet\n";
    } else {
        for (const auto& [ip, entry] : online_list) {
            if (entry.online == false) {
                continue;
            }
            std::string time_str;
            auto timestamp
                = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>(
                    std::chrono::milliseconds(entry.timestamp_ms));
            std::time_t time_now = std::chrono::system_clock::to_time_t(timestamp);
            std::cout << " │ " << std::left << std::setw(15) << entry.username.substr(0, 15)
                      << " │ " << std::left << std::setw(18) << entry.ip_address.substr(0, 18)
                      << " │ " << std::put_time(std::localtime(&time_now), "%Y-%m-%d %H:%M:%S")
                      << "\n";
        }
    }
    std::cout << "\n";
    std::cout << std::flush;
}

} // namespace cli
