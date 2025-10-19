#include "core/executor.h"
#include "discovery/heartbeat.h"
#include "discovery/online_list_inspector.h"
#include "util/settings.h"
#include <asio/steady_timer.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/use_future.hpp>
#include <chrono>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <thread>

using namespace std::chrono_literals;

namespace discovery {
namespace {

class DiscoveryIntegrationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        util::Settings::instance().init("test_settings.json");
        spdlog::set_level(spdlog::level::info);
    }

    void TearDown() override { std::remove("test_settings.json"); }
};

TEST_F(DiscoveryIntegrationTest, HeartbeatAndInspectorIntegration) {
    core::Executor executor;
    Heartbeat heartbeat(executor, 100); // 100ms 间隔
    OnlineListInspector inspector(executor);

    auto test_future = executor.spawn(
        [&]() -> asio::awaitable<bool> {
            try {
                inspector.start();
                executor.spawn(heartbeat.start());

                asio::steady_timer timer(co_await asio::this_coro::executor);
                timer.expires_after(500ms);
                co_await timer.async_wait(asio::use_awaitable);

                auto online_list = inspector.get_online_list();
                spdlog::info("Online list size (show_self=false): {}", online_list.size());

                inspector.show_self_ = true;

                timer.expires_after(300ms);
                co_await timer.async_wait(asio::use_awaitable);

                online_list = inspector.get_online_list();
                spdlog::info("Online list size (show_self=true): {}", online_list.size());
                bool found_self = false;
                for (const auto& [ip, entry] : online_list) {
                    spdlog::info("Found user: {} at {}, online: {}",
                                 entry.username,
                                 ip,
                                 entry.online);
                    if (entry.online) {
                        found_self = true;
                    }
                }

                co_await heartbeat.stop();
                inspector.stop();

                co_return found_self;
            } catch (const std::exception& e) {
                spdlog::error("Exception in test: {}", e.what());
                co_return false;
            }
        },
        asio::use_future);

    std::thread runner([&executor]() {
        try {
            executor.start();
        } catch (const std::exception& e) {
            spdlog::error("Exception in executor thread: {}", e.what());
        }
    });

    bool result = false;
    try {
        result = test_future.get();
    } catch (const std::exception& e) {
        spdlog::error("Exception getting future result: {}", e.what());
        result = false;
    }

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }

    ASSERT_TRUE(result) << "Failed to detect self in online list";
}

TEST_F(DiscoveryIntegrationTest, OnlineListShowSelfToggle) {
    core::Executor executor;
    Heartbeat heartbeat(executor, 100);
    OnlineListInspector inspector(executor);

    auto test_future = executor.spawn(
        [&]() -> asio::awaitable<bool> {
            try {
                inspector.start();
                executor.spawn(heartbeat.start());

                asio::steady_timer timer(co_await asio::this_coro::executor);
                timer.expires_after(300ms);
                co_await timer.async_wait(asio::use_awaitable);

                auto list1 = inspector.get_online_list();
                int count1 = 0;
                for (const auto& [_, entry] : list1) {
                    if (entry.online)
                        count1++;
                }
                spdlog::info("Online count (show_self=false): {}", count1);

                inspector.show_self_ = true;
                timer.expires_after(300ms);
                co_await timer.async_wait(asio::use_awaitable);

                auto list2 = inspector.get_online_list();
                int count2 = 0;
                for (const auto& [_, entry] : list2) {
                    if (entry.online)
                        count2++;
                }
                spdlog::info("Online count (show_self=true): {}", count2);

                co_await heartbeat.stop();
                inspector.stop();

                co_return (count1 == 0) && (count2 > 0);
            } catch (const std::exception& e) {
                spdlog::error("Exception: {}", e.what());
                co_return false;
            }
        },
        asio::use_future);

    std::thread runner([&executor]() { executor.start(); });

    bool result = test_future.get();

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }

    ASSERT_TRUE(result) << "show_self flag toggle test failed";
}

TEST_F(DiscoveryIntegrationTest, UserTimeout) {
    core::Executor executor;
    Heartbeat heartbeat(executor, 100);
    OnlineListInspector inspector(executor);
    inspector.show_self_ = true;

    auto test_future = executor.spawn(
        [&]() -> asio::awaitable<bool> {
            try {
                inspector.start();
                executor.spawn(heartbeat.start());

                asio::steady_timer timer(co_await asio::this_coro::executor);
                timer.expires_after(300ms);
                co_await timer.async_wait(asio::use_awaitable);

                auto list1 = inspector.get_online_list();
                int online_count1 = 0;
                for (const auto& [_, entry] : list1) {
                    if (entry.online)
                        online_count1++;
                }
                spdlog::info("Online users before stop: {}", online_count1);

                co_await heartbeat.stop();

                timer.expires_after(6s);
                co_await timer.async_wait(asio::use_awaitable);

                auto list2 = inspector.get_online_list();
                int online_count2 = 0;
                for (const auto& [_, entry] : list2) {
                    spdlog::info("User status: {} online={}", entry.username, entry.online);
                    if (entry.online)
                        online_count2++;
                }
                spdlog::info("Online users after timeout: {}", online_count2);

                inspector.stop();
                co_return (online_count1 > 0) && (online_count2 == 0);
            } catch (const std::exception& e) {
                spdlog::error("Exception: {}", e.what());
                co_return false;
            }
        },
        asio::use_future);

    std::thread runner([&executor]() { executor.start(); });

    bool result = test_future.get();

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }

    ASSERT_TRUE(result) << "User timeout test failed";
}

} // namespace
} // namespace discovery
