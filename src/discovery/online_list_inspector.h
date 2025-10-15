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
    explicit OnlineListInspector(core::Executor& executor)
        : executor_(executor)
        , socket_(executor.get_io_context())
        , receiver_(executor, socket_) {};
    ~OnlineListInspector() = default;

    OnlineListInspector(const OnlineListInspector&) = delete;
    OnlineListInspector& operator=(const OnlineListInspector&) = delete;

    void start();
    void stop();
    void restart();
    struct UserEntry {
        std::string ip_address;
        std::string username;
        std::chrono::steady_clock::time_point last_heartbeat_time;
        std::chrono::steady_clock::time_point expire_time;
    };

    std::map<std::string, UserEntry> get_online_list() {
        std::lock_guard<std::mutex> lock(mutex_);
        return online_list_;
    }

  private:
    constexpr static int kTimeoutThresholdMs = 3000;

    asio::awaitable<void> inspect_loop();
    void update_online_list(HeartbeatRequest&& msg);
    asio::awaitable<void> remove_user(const std::string& ip);

    std::map<std::string, UserEntry> online_list_;
    core::Executor& executor_;
    asio::ip::udp::socket socket_;
    core::net::io::UdpReceiver receiver_;

    std::mutex mutex_;
};
} // namespace discovery