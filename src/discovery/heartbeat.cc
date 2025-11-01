#include "heartbeat.h"
#include "core/timer/spawn_after_delay.h"
#include "heartbeat.pb.h"
#include "util/settings.h"
#include <asio/ip/host_name.hpp>
#include <asio/ip/tcp.hpp>
#include <chrono>
#include <spdlog/spdlog.h>

namespace discovery {

asio::awaitable<void> Heartbeat::start() {
    if (running_.exchange(true)) {
        co_return;
    }

    HeartbeatRequest heartbeat_msg;
    asio::ip::tcp::resolver resolver(executor_.get_io_context());
    std::string local_host = asio::ip::host_name();
    auto endpoints = resolver.resolve(asio::ip::tcp::v4(), local_host, "");
    std::string local_ip = endpoints.begin()->endpoint().address().to_string();
    spdlog::info("Heartbeat using local IP: {}", local_ip);
    heartbeat_msg.set_ip_address(local_ip);

    std::string username = util::Settings::instance().get().value("username", "unknown");
    heartbeat_msg.set_username(username);

    while (running_) {
        auto now = std::chrono::system_clock::now();
        auto timestamp_ms
            = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        heartbeat_msg.set_timestamp_ms(timestamp_ms);

        co_await core::timer::spawn_after_delay(sender_.send_message_to(heartbeat_msg),
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