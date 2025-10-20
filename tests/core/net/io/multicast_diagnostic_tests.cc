#include "core/executor.h"
#include "core/net/io/udp_receiver.h"
#include "core/net/io/udp_sender.h"
#include "core/timer/spawn_with_timeout.h"
#include <asio/use_future.hpp>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <span>
#include <string>

using namespace std::chrono_literals;

namespace {

// 专门测试 multicast loopback 的诊断测试
TEST(MulticastDiagnostic, LoopbackTest) {
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("=== Multicast Loopback Diagnostic Test ===");
    
    core::Executor executor;
    asio::ip::udp::socket receiver_socket(executor.get_io_context());
    asio::ip::udp::socket sender_socket(executor.get_io_context());
    
    core::net::io::UdpReceiver receiver(executor, receiver_socket);
    core::net::io::UdpSender sender(executor, sender_socket);

    std::array<std::byte, 256> buffer{};
    core::net::io::MutDataBlock buffer_span(buffer.data(), buffer.size());
    const std::string payload_text = "multicast-test-message";

    auto test_future = executor.spawn(
        [&]() -> asio::awaitable<bool> {
            try {
                // 发送到 multicast 地址
                auto payload_span = std::span<const char>(payload_text.data(), payload_text.size());
                auto payload_bytes = std::as_bytes(payload_span);
                
                spdlog::info("Sending multicast packet...");
                co_await sender.send_to(payload_bytes); // 使用默认 multicast 地址
                
                spdlog::info("Waiting for multicast packet reception...");
                auto result = co_await core::timer::spawn_with_timeout(
                    receiver.receive(buffer_span), 2s);

                if (!result.has_value()) {
                    spdlog::error("FAILED: Timeout waiting for multicast packet");
                    spdlog::error("This indicates multicast loopback is not working on this platform");
                    co_return false;
                }

                const auto bytes = result.value();
                spdlog::info("SUCCESS: Received {} bytes", bytes);
                
                std::string received(reinterpret_cast<const char*>(buffer_span.data()),
                                    buffer_span.size());
                spdlog::info("Received data: {}", received);
                
                if (received == payload_text) {
                    spdlog::info("SUCCESS: Data matches!");
                    co_return true;
                } else {
                    spdlog::error("FAILED: Data mismatch");
                    co_return false;
                }
            } catch (const std::exception& e) {
                spdlog::error("EXCEPTION: {}", e.what());
                co_return false;
            }
        },
        asio::use_future);

    std::thread runner([&executor]() { executor.start(); });

    bool result = false;
    try {
        result = test_future.get();
    } catch (const std::exception& e) {
        spdlog::error("Future exception: {}", e.what());
    }

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }

    spdlog::info("=== Test Result: {} ===", result ? "PASSED" : "FAILED");
    
    // 在 macOS 上，如果这个测试失败，说明系统不支持 multicast loopback
    // 我们应该考虑使用其他方案（如直接 UDP 通信）
    ASSERT_TRUE(result) << "Multicast loopback not working - platform may not support it";
}

} // namespace
