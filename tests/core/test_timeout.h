#pragma once

#include "core/executor.h"
#include <asio/awaitable.hpp>
#include <asio/error_code.hpp>
#include <asio/redirect_error.hpp>
#include <asio/steady_timer.hpp>
#include <asio/use_awaitable.hpp>
#include <atomic>
#include <chrono>

namespace test_utils {
class TimeoutGuard {
  public:
    explicit TimeoutGuard(core::Executor& executor,
                          std::chrono::milliseconds timeout = std::chrono::seconds(5))
        : executor_(executor)
        , timer_(executor.get_io_context()) {
        timer_.expires_after(timeout);
        executor_.spawn([this]() -> asio::awaitable<void> {
            asio::error_code ec;
            co_await timer_.async_wait(asio::redirect_error(asio::use_awaitable, ec));
            if (!ec && active_.load(std::memory_order_acquire)) {
                timed_out_.store(true, std::memory_order_release);
                executor_.stop();
            }
            active_.store(false, std::memory_order_release);
            co_return;
        });
    }

    ~TimeoutGuard() { cancel(); }

    void cancel() {
        bool expected = true;
        if (active_.compare_exchange_strong(expected, false)) {
            timer_.cancel();
        }
    }

    [[nodiscard]] bool timed_out() const { return timed_out_.load(std::memory_order_acquire); }

  private:
    core::Executor& executor_;
    asio::steady_timer timer_;
    std::atomic_bool active_{true};
    std::atomic_bool timed_out_{false};
};
} // namespace test_utils
