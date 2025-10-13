#pragma once

#include <asio/awaitable.hpp>
#include <asio/error_code.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <asio/redirect_error.hpp>
#include <asio/steady_timer.hpp>
#include <asio/use_awaitable.hpp>
#include <chrono>
#include <optional>
#include <type_traits>
#include <utility>

namespace core::timer {

using namespace asio::experimental::awaitable_operators;

template<typename Awaitable>
    requires(!std::is_void_v<typename Awaitable::value_type>)
inline auto spawn_with_timeout(Awaitable awaitable_task, std::chrono::steady_clock::duration timeout)
    -> asio::awaitable<std::optional<typename Awaitable::value_type>> {
    using T = typename Awaitable::value_type;

    auto timeout_impl = [timeout]() -> asio::awaitable<std::optional<T>> {
        asio::steady_timer timer(co_await asio::this_coro::executor);
        timer.expires_after(timeout);
        asio::error_code ec;
        co_await timer.async_wait(asio::redirect_error(asio::use_awaitable, ec));
        co_return std::nullopt;
    };

    auto task_impl = [](Awaitable task) -> asio::awaitable<std::optional<T>> {
        co_return co_await std::move(task);
    };

    auto result = co_await (timeout_impl() || task_impl(std::move(awaitable_task)));

    co_return result.index() == 0 ? std::get<0>(result) : std::get<1>(result);
}

// Specialization for void return type - returns bool (true = success, false = timeout)
template<typename Awaitable>
    requires(std::is_void_v<typename Awaitable::value_type>)
inline auto spawn_with_timeout(Awaitable awaitable_task, std::chrono::steady_clock::duration timeout)
    -> asio::awaitable<bool> {
    auto timeout_impl = [timeout]() -> asio::awaitable<bool> {
        asio::steady_timer timer(co_await asio::this_coro::executor);
        timer.expires_after(timeout);
        asio::error_code ec;
        co_await timer.async_wait(asio::redirect_error(asio::use_awaitable, ec));
        co_return false; // Timeout
    };

    auto task_impl = [](Awaitable task) -> asio::awaitable<bool> {
        co_await std::move(task);
        co_return true; // Success
    };

    auto result = co_await (timeout_impl() || task_impl(std::move(awaitable_task)));

    co_return result.index() == 0 ? std::get<0>(result) : std::get<1>(result);
}
} // namespace core::timer