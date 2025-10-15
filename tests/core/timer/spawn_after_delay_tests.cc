#include "core/timer/spawn_after_delay.h"
#include "timer_test_fixture.h"
#include <asio/awaitable.hpp>
#include <chrono>
#include <future>

using namespace std::chrono_literals;

TEST_F(TimerTest, SpawnAfterDelayWithCallable) {
    std::promise<void> done;
    auto future = done.get_future();
    auto start = std::chrono::steady_clock::now();

    executor.spawn([&done, start]() -> asio::awaitable<void> {
        co_await core::timer::spawn_after_delay(
            [&done, start]() -> asio::awaitable<void> {
                auto elapsed = std::chrono::steady_clock::now() - start;
                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                                      .count();
                EXPECT_GE(elapsed_ms, 90);
                EXPECT_LE(elapsed_ms, 150);

                done.set_value();
                co_return;
            },
            100);
        co_return;
    });

    ASSERT_EQ(future.wait_for(500ms), std::future_status::ready);
}

TEST_F(TimerTest, SpawnAfterDelayWithAwaitable) {
    std::promise<int> result_promise;
    auto future = result_promise.get_future();
    auto start = std::chrono::steady_clock::now();

    executor.spawn([&result_promise, start]() -> asio::awaitable<void> {
        auto task = [&result_promise, start]() -> asio::awaitable<void> {
            auto elapsed = std::chrono::steady_clock::now() - start;
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

            EXPECT_GE(elapsed_ms, 140);
            EXPECT_LE(elapsed_ms, 200);

            result_promise.set_value(42);
            co_return;
        };

        co_await core::timer::spawn_after_delay(task(), 150);
        co_return;
    });

    ASSERT_EQ(future.wait_for(500ms), std::future_status::ready);
    EXPECT_EQ(future.get(), 42);
}

TEST_F(TimerTest, SpawnAfterDelayZeroDelay) {
    std::promise<void> done;
    auto future = done.get_future();

    executor.spawn([&done]() -> asio::awaitable<void> {
        co_await core::timer::spawn_after_delay(
            [&done]() -> asio::awaitable<void> {
                done.set_value();
                co_return;
            },
            0);
        co_return;
    });

    ASSERT_EQ(future.wait_for(100ms), std::future_status::ready);
}

TEST_F(TimerTest, MultipleDelaysSequential) {
    std::promise<int> counter_promise;
    auto future = counter_promise.get_future();

    executor.spawn([&counter_promise]() -> asio::awaitable<void> {
        int count = 0;

        co_await core::timer::spawn_after_delay(
            [&count]() -> asio::awaitable<void> {
                count++;
                co_return;
            },
            50);

        co_await core::timer::spawn_after_delay(
            [&count]() -> asio::awaitable<void> {
                count++;
                co_return;
            },
            50);

        co_await core::timer::spawn_after_delay(
            [&count]() -> asio::awaitable<void> {
                count++;
                co_return;
            },
            50);

        counter_promise.set_value(count);
        co_return;
    });

    ASSERT_EQ(future.wait_for(1000ms), std::future_status::ready);
    EXPECT_EQ(future.get(), 3);
}

TEST_F(TimerTest, NestedTimers) {
    std::promise<std::string> result_promise;
    auto future = result_promise.get_future();

    executor.spawn([&result_promise]() -> asio::awaitable<void> {
        std::string result = "start";

        co_await core::timer::spawn_after_delay(
            [&result]() -> asio::awaitable<void> {
                result += "-outer";
                co_await core::timer::spawn_after_delay(
                    [&result]() -> asio::awaitable<void> {
                        result += "-inner";
                        co_return;
                    },
                    50);
                co_return;
            },
            50);

        result_promise.set_value(result);
        co_return;
    });

    ASSERT_EQ(future.wait_for(1000ms), std::future_status::ready);
    EXPECT_EQ(future.get(), "start-outer-inner");
}
