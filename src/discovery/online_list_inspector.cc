#include "online_list_inspector.h"
#include "heartbeat.pb.h"
#include <asio/ip/host_name.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>
#include <spdlog/spdlog.h>
#include <string>

namespace discovery {

OnlineListInspector::OnlineListInspector(core::Executor& executor)
    : executor_(executor)
    , socket_(executor.get_io_context())
    , receiver_(executor, socket_) {}

void OnlineListInspector::start() {
    if (running_.exchange(true)) {
        return;
    }

    local_ip_ = get_local_ip();
    executor_.spawn(inspect_loop());
    executor_.spawn(cleanup_loop());
}

void OnlineListInspector::stop() {
    running_.store(false);
    socket_.close();
    std::lock_guard<std::mutex> lock(mutex_);
    online_list_.clear();
}

void OnlineListInspector::restart() {
    stop();
    start();
}

asio::awaitable<void> OnlineListInspector::inspect_loop() {
    std::array<std::byte, 512> buffer{};
    core::net::io::MutDataBlock buffer_span(buffer.data(), buffer.size());

    while (running_.load()) {
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

        update_online_list(std::move(heartbeat_msg));
    }
}

void OnlineListInspector::update_online_list(HeartbeatRequest&& msg) {
    auto ip = msg.ip_address();

    if (!show_self_.load() && !local_ip_.empty() && ip == local_ip_) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();

    if (!online_list_.contains(ip) || !online_list_[ip].online) {
        UserEntry entry;
        entry.ip_address = ip;
        entry.username = msg.username();
        entry.timestamp_ms = msg.timestamp_ms();
        entry.last_heartbeat_time = now;
        entry.expire_time = now + std::chrono::milliseconds(kTimeoutThresholdMs);
        entry.online = true;
        online_list_[ip] = std::move(entry);
        spdlog::info("User online: {} ({})", msg.username(), ip);
    } else {
        auto& entry = online_list_[ip];
        entry.timestamp_ms = msg.timestamp_ms();
        entry.last_heartbeat_time = now;
        entry.expire_time = now + std::chrono::milliseconds(kTimeoutThresholdMs);
    }
}

asio::awaitable<void> OnlineListInspector::cleanup_loop() {
    while (running_.load()) {
        asio::steady_timer timer(co_await asio::this_coro::executor,
                                 std::chrono::milliseconds(kCleanupIntervalMs));
        co_await timer.async_wait(asio::use_awaitable);

        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& [ip, entry] : online_list_) {
            if (entry.online && now >= entry.expire_time) {
                spdlog::info("User offline (timeout): {} ({})", entry.username, ip);
                entry.online = false;
            }
        }
    }
}

asio::awaitable<void> OnlineListInspector::remove_user(std::string ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (online_list_.contains(ip)) {
        auto now = std::chrono::steady_clock::now();
        if (now >= online_list_[ip].expire_time) {
            spdlog::debug("Removing expired user: {}", ip);
            online_list_[ip].online = false;
        }
    }
    co_return;
}

std::string OnlineListInspector::get_local_ip() {
    try {
        asio::ip::tcp::resolver resolver(executor_.get_io_context());
        std::string local_host = asio::ip::host_name();
        auto endpoints = resolver.resolve(asio::ip::tcp::v6(), local_host, "");
        std::string local_ip = endpoints.begin()->endpoint().address().to_string();
        spdlog::info("Local IP detected: {}", local_ip);
        return local_ip;
    } catch (const std::exception& e) {
        spdlog::warn("Failed to get local IP: {}", e.what());
        return "";
    }
}

} // namespace discovery