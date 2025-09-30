#pragma once

#include "core/executor.h"
#include <asio/awaitable.hpp>
#include <asio/error.hpp>
#include <asio/error_code.hpp>
#include <asio/redirect_error.hpp>
#include <asio/steady_timer.hpp>
#include <asio/use_awaitable.hpp>
#include <chrono>
#include <memory>
#include <utility>
namespace core::timer {
class Timer {
  public:
    explicit Timer(core::Executor& executor)
        : executor_(executor) {}

    ~Timer() = default;

    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    template<typename Awaitable>
    void spawn_after_delay(Awaitable&& awaitable, int delay_ms);
    template<typename Awaitable>
    void spawn_at_fixed_rate(Awaitable&& awaitable, int initial_delay_ms, int interval_ms);

  private:
    core::Executor& executor_;
};

template<typename Awaitable>
void Timer::spawn_after_delay(Awaitable&& awaitable, int delay_ms) {
    auto timer = std::make_shared<asio::steady_timer>(executor_.get_io_context(),
                                                      std::chrono::milliseconds(delay_ms));
    executor_.spawn(
        [timer, awaitable = std::forward<Awaitable>(awaitable)]() mutable -> asio::awaitable<void> {
            asio::error_code ec;
            co_await timer->async_wait(asio::redirect_error(asio::use_awaitable, ec));
            if (!ec) {
                co_await awaitable();
            }
        });
}

template<typename Awaitable>
void Timer::spawn_at_fixed_rate(Awaitable&& awaitable, int initial_delay_ms, int interval_ms) {
    auto timer = std::make_shared<asio::steady_timer>(executor_.get_io_context());
    timer->expires_after(std::chrono::milliseconds(initial_delay_ms));
    executor_.spawn([timer,
                     awaitable = std::forward<Awaitable>(awaitable),
                     interval_ms]() mutable -> asio::awaitable<void> {
        asio::error_code ec;
        while (true) {
            co_await timer->async_wait(asio::redirect_error(asio::use_awaitable, ec));
            if (ec == asio::error::operation_aborted) {
                co_return;
            }
            if (!ec) {
                co_await awaitable();
            }
            timer->expires_after(std::chrono::milliseconds(interval_ms));
        }
    });
}
} // namespace core::timer