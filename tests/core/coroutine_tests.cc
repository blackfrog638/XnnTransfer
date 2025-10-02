#include "core/timer.h"
#include "gtest/gtest.h"
#include <asio/awaitable.hpp>
#include <chrono>
#include <core/executor.h>
#include <future>

using namespace std::chrono_literals;

TEST(CoroutineTest, ExecutorTest) {
    core::Executor executor;
    std::promise<void> done;
    auto future = done.get_future();

    executor.spawn([&done]() -> asio::awaitable<void> {
        done.set_value();
        co_return;
    });

    std::thread runner([&executor]() { executor.start(); });

    ASSERT_EQ(future.wait_for(500ms), std::future_status::ready);

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}

TEST(CoroutineTest, TimerTest) {
    core::Executor executor;
    core::timer::Timer timer(executor);
    std::promise<void> done;
    auto future = done.get_future();

    timer.spawn_after_delay(
        [&done]() -> asio::awaitable<void> {
            done.set_value();
            co_return;
        },
        100);

    std::thread runner([&executor]() { executor.start(); });

    ASSERT_EQ(future.wait_for(500ms), std::future_status::ready);

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}
