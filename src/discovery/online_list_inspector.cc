#include "online_list_inspector.h"
#include "core/timer/spawn_at.h"
#include "heartbeat.pb.h"
#include <spdlog/spdlog.h>
#include <string>

namespace discovery {

void OnlineListInspector::start() {
    socket_.open(asio::ip::udp::v4());
    executor_.spawn(inspect_loop());
}

void OnlineListInspector::stop() {
    socket_.close();
    online_list_.clear();
}

void OnlineListInspector::restart() {
    stop();
    start();
}

asio::awaitable<void> OnlineListInspector::inspect_loop() {
    std::array<std::byte, 512> buffer{};
    core::net::io::MutDataBlock buffer_span(buffer.data(), buffer.size());

    constexpr std::uint16_t kPort = 40101;

    while (true) {
        co_await receiver_.receive(buffer_span);
        if (buffer_span.empty()) {
            continue;
        }

        std::string serialized_data(reinterpret_cast<const char*>(buffer_span.data()),
                                    buffer_span.size());

        HeartbeatRequest heartbeat_msg;
        if (!heartbeat_msg.ParseFromString(serialized_data)) {
            spdlog::warn("Failed to parse HeartbeatRequest from received data");
            continue;
        }

        spdlog::debug("Received heartbeat: username={}, ip={}, timestamp={}",
                      heartbeat_msg.username(),
                      heartbeat_msg.ip_address(),
                      heartbeat_msg.timestamp_ms());
    }
}

void OnlineListInspector::update_online_list(HeartbeatRequest&& msg) {
    if (!online_list_.contains(msg.ip_address())) {
        mutex_.lock();
        UserEntry entry;
        entry.ip_address = msg.ip_address();
        entry.username = msg.username();
        entry.last_heartbeat_time = std::chrono::steady_clock::now();
        entry.expire_time = entry.last_heartbeat_time
                            + std::chrono::milliseconds(kTimeoutThresholdMs);
        online_list_[msg.ip_address()] = entry;
        mutex_.unlock();
        executor_.spawn(core::timer::spawn_at(remove_user(msg.ip_address()), entry.expire_time));

    } else {
        mutex_.lock();
        auto& entry = online_list_[msg.ip_address()];
        entry.last_heartbeat_time = std::chrono::steady_clock::now();
        entry.expire_time = entry.last_heartbeat_time
                            + std::chrono::milliseconds(kTimeoutThresholdMs);
        mutex_.unlock();
    }
}

asio::awaitable<void> OnlineListInspector::remove_user(const std::string& ip) {
    mutex_.lock();
    if (online_list_.contains(ip)) {
        auto now = std::chrono::steady_clock::now();
        if (now >= online_list_[ip].expire_time) {
            online_list_.erase(ip);
        }
    }
    mutex_.unlock();
    co_return;
}

} // namespace discovery