#include "core/executor.h"
#include "core/net/io/udp_receiver.h"
#include "core/net/io/udp_sender.h"
#include "core/timer/spawn_with_timeout.h"
#include <asio/use_future.hpp>
#include <gtest/gtest.h>
#include <span>
#include <spdlog/spdlog.h>
#include <string>

using namespace std::chrono_literals;

namespace {

// 专门测试 multicast loopback 的诊断测试
// 注意：某些 CI 环境（如 GitHub Actions macOS）不支持 multicast
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
                auto result = co_await core::timer::spawn_with_timeout(receiver.receive(buffer_span),
                                                                       2s);

                if (!result.has_value()) {
                    spdlog::error("FAILED: Timeout waiting for multicast packet");
                    spdlog::error(
                        "This indicates multicast loopback is not working on this platform");
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
                std::string error_msg = e.what();
                spdlog::error("EXCEPTION: {}", error_msg);

                // "No route to host" 表示平台不支持 multicast
                if (error_msg.find("No route to host") != std::string::npos) {
                    spdlog::warn("Platform does not support multicast - this is expected on some "
                                 "CI environments");
                }
                co_return false;
            }
        },
        asio::use_future);

    std::thread runner([&executor]() { executor.start(); });

    bool result = false;
    bool is_platform_limitation = false;
    try {
        result = test_future.get();
    } catch (const std::exception& e) {
        std::string error_msg = e.what();
        spdlog::error("Future exception: {}", error_msg);
        if (error_msg.find("No route to host") != std::string::npos) {
            is_platform_limitation = true;
        }
    }

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }

    spdlog::info("=== Test Result: {} ===", result ? "PASSED" : "FAILED");

    // 如果是平台限制（如 GitHub Actions macOS），跳过测试而不是失败
    if (!result && is_platform_limitation) {
        spdlog::warn("SKIP: Multicast not supported on this platform/environment");
        GTEST_SKIP() << "Multicast not supported - known limitation on GitHub Actions macOS";
    }

    ASSERT_TRUE(result) << "Multicast loopback test failed";
}
} // namespace
