#include "core/timer/spawn_at.h"
#include "timer_test_fixture.h"
#include <asio/awaitable.hpp>
#include <chrono>
#include <future>

using namespace std::chrono_literals;

TEST_F(TimerTest, SpawnAtWithCallable) {
    std::promise<void> done;
    auto future = done.get_future();
    auto target_time = std::chrono::steady_clock::now() + 100ms;

    executor.spawn([&done, target_time]() -> asio::awaitable<void> {
        co_await core::timer::spawn_at(
            [&done]() -> asio::awaitable<void> {
                done.set_value();
                co_return;
            },
            target_time);
        co_return;
    });

    ASSERT_EQ(future.wait_for(500ms), std::future_status::ready);
}

TEST_F(TimerTest, SpawnAtWithAwaitable) {
    std::promise<std::string> result_promise;
    auto future = result_promise.get_future();
    auto target_time = std::chrono::steady_clock::now() + 120ms;

    executor.spawn([&result_promise, target_time]() -> asio::awaitable<void> {
        auto task = [&result_promise]() -> asio::awaitable<void> {
            result_promise.set_value("completed");
            co_return;
        };

        co_await core::timer::spawn_at(task(), target_time);
        co_return;
    });

    ASSERT_EQ(future.wait_for(500ms), std::future_status::ready);
    EXPECT_EQ(future.get(), "completed");
}

TEST_F(TimerTest, SpawnAtPastTime) {
    std::promise<void> done;
    auto future = done.get_future();
    auto past_time = std::chrono::steady_clock::now() - 100ms;

    executor.spawn([&done, past_time]() -> asio::awaitable<void> {
        co_await core::timer::spawn_at(
            [&done]() -> asio::awaitable<void> {
                done.set_value();
                co_return;
            },
            past_time);
        co_return;
    });

    ASSERT_EQ(future.wait_for(100ms), std::future_status::ready);
}
