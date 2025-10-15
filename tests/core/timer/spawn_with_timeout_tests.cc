#include "core/timer/spawn_with_timeout.h"
#include "timer_test_fixture.h"
#include <asio/awaitable.hpp>
#include <asio/steady_timer.hpp>
#include <asio/use_awaitable.hpp>
#include <chrono>
#include <future>
#include <optional>
#include <string>


using namespace std::chrono_literals;

TEST_F(TimerTest, SpawnWithTimeoutSuccessNonVoid) {
    std::promise<std::optional<int>> result_promise;
    auto future = result_promise.get_future();

    executor.spawn([&result_promise]() -> asio::awaitable<void> {
        auto task = []() -> asio::awaitable<int> {
            asio::steady_timer timer(co_await asio::this_coro::executor, 50ms);
            co_await timer.async_wait(asio::use_awaitable);
            co_return 123;
        };

        auto result = co_await core::timer::spawn_with_timeout(task(), 200ms);
        result_promise.set_value(result);
        co_return;
    });

    ASSERT_EQ(future.wait_for(500ms), std::future_status::ready);
    auto result = future.get();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 123);
}

TEST_F(TimerTest, SpawnWithTimeoutTimeoutNonVoid) {
    std::promise<std::optional<int>> result_promise;
    auto future = result_promise.get_future();

    executor.spawn([&result_promise]() -> asio::awaitable<void> {
        auto task = []() -> asio::awaitable<int> {
            asio::steady_timer timer(co_await asio::this_coro::executor, 300ms);
            co_await timer.async_wait(asio::use_awaitable);
            co_return 456;
        };

        auto result = co_await core::timer::spawn_with_timeout(task(), 100ms);
        result_promise.set_value(result);
        co_return;
    });

    ASSERT_EQ(future.wait_for(500ms), std::future_status::ready);
    auto result = future.get();
    EXPECT_FALSE(result.has_value()); // Should timeout
}

TEST_F(TimerTest, SpawnWithTimeoutSuccessVoid) {
    std::promise<bool> result_promise;
    auto future = result_promise.get_future();

    executor.spawn([&result_promise]() -> asio::awaitable<void> {
        auto task = []() -> asio::awaitable<void> {
            asio::steady_timer timer(co_await asio::this_coro::executor, 50ms);
            co_await timer.async_wait(asio::use_awaitable);
            co_return;
        };

        bool result = co_await core::timer::spawn_with_timeout(task(), 200ms);
        result_promise.set_value(result);
        co_return;
    });

    ASSERT_EQ(future.wait_for(500ms), std::future_status::ready);
    EXPECT_TRUE(future.get()); // Should complete successfully
}

TEST_F(TimerTest, SpawnWithTimeoutTimeoutVoid) {
    std::promise<bool> result_promise;
    auto future = result_promise.get_future();

    executor.spawn([&result_promise]() -> asio::awaitable<void> {
        auto task = []() -> asio::awaitable<void> {
            asio::steady_timer timer(co_await asio::this_coro::executor, 300ms);
            co_await timer.async_wait(asio::use_awaitable);
            co_return;
        };

        bool result = co_await core::timer::spawn_with_timeout(task(), 100ms);
        result_promise.set_value(result);
        co_return;
    });

    ASSERT_EQ(future.wait_for(500ms), std::future_status::ready);
    EXPECT_FALSE(future.get()); // Should timeout
}

TEST_F(TimerTest, SpawnWithTimeoutImmediateCompletion) {
    std::promise<std::optional<std::string>> result_promise;
    auto future = result_promise.get_future();

    executor.spawn([&result_promise]() -> asio::awaitable<void> {
        auto task = []() -> asio::awaitable<std::string> { co_return "immediate"; };

        auto result = co_await core::timer::spawn_with_timeout(task(), 1000ms);
        result_promise.set_value(result);
        co_return;
    });

    ASSERT_EQ(future.wait_for(500ms), std::future_status::ready);
    auto result = future.get();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "immediate");
}
