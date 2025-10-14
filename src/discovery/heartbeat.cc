#include "heartbeat.h"
#include "core/timer.h"
#include "heartbeat.pb.h"
#include <asio/ip/host_name.hpp>
#include <chrono>
#include <spdlog/spdlog.h>

namespace discovery {

asio::awaitable<void> Heartbeat::start() {
    if (running_.exchange(true)) {
        co_return;
    }

    std::string local_ip = asio::ip::host_name();
    spdlog::info("Heartbeat using local IP: {}", local_ip);

    while (running_) {
        HeartbeatRequest heartbeat_msg;
        heartbeat_msg.set_ip_address(local_ip);

        auto now = std::chrono::system_clock::now();
        auto timestamp_ms
            = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        heartbeat_msg.set_timestamp_ms(timestamp_ms);

        std::string serialized = heartbeat_msg.SerializeAsString();
        co_await core::timer::spawn_after_delay(sender_.send_to(std::as_bytes(
                                                    std::span<const char>(serialized.data(),
                                                                          serialized.size()))),
                                                interval_ms_);

        spdlog::debug("Heartbeat sent: ip={}, timestamp={}", local_ip, timestamp_ms);
    }
}

asio::awaitable<void> Heartbeat::stop() {
    running_.store(false);
    co_return;
}

asio::awaitable<void> Heartbeat::restart() {
    co_await stop();
    co_await start();
}

} // namespace discovery