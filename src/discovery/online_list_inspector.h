#pragma once

#include "asio/awaitable.hpp"
#include "asio/ip/udp.hpp"
#include "core/executor.h"
#include "core/net/io/udp_receiver.h"
#include "heartbeat.pb.h"
#include <map>
#include <mutex>
#include <string>

namespace discovery {
class OnlineListInspector {
  public:
    explicit OnlineListInspector(core::Executor& executor);

    ~OnlineListInspector() = default;

    OnlineListInspector(const OnlineListInspector&) = delete;
    OnlineListInspector& operator=(const OnlineListInspector&) = delete;

    void start();
    void stop();
    void restart();

    std::atomic<bool> show_self_{false}; // 功能开关：是否显示本机设备

    struct UserEntry {
        std::string ip_address;
        std::string username;
        int64_t timestamp_ms;
        std::chrono::steady_clock::time_point last_heartbeat_time;
        std::chrono::steady_clock::time_point expire_time;
        bool online{false};
    };

    std::map<std::string, UserEntry> get_online_list() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return online_list_;
    }

  private:
    constexpr static int kTimeoutThresholdMs = 5000;
    constexpr static int kCleanupIntervalMs = 1000;

    asio::awaitable<void> inspect_loop();
    asio::awaitable<void> cleanup_loop();
    void update_online_list(HeartbeatRequest&& msg);
    asio::awaitable<void> remove_user(std::string ip);
    std::string get_local_ip();

    std::map<std::string, UserEntry> online_list_;
    core::Executor& executor_;
    asio::ip::udp::socket socket_;
    core::net::io::UdpReceiver receiver_;
    std::atomic<bool> running_{false};
    std::string local_ip_;

    mutable std::mutex mutex_;
};
} // namespace discovery