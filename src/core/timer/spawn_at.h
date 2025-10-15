#pragma once

#include <asio/awaitable.hpp>
#include <asio/error.hpp>
#include <asio/error_code.hpp>
#include <asio/redirect_error.hpp>
#include <asio/steady_timer.hpp>
#include <asio/use_awaitable.hpp>
namespace core::timer {

template<typename Callable>
    requires std::invocable<Callable>
inline asio::awaitable<void> spawn_at(Callable&& callable, auto& timestamp) {
    asio::steady_timer timer(co_await asio::this_coro::executor);
    timer.expires_at(timestamp);
    asio::error_code ec;
    co_await timer.async_wait(asio::redirect_error(asio::use_awaitable, ec));
    if (!ec) {
        co_await callable();
    }
}

template<typename Awaitable>
    requires(!std::invocable<Awaitable>)
inline asio::awaitable<void> spawn_at(Awaitable&& awaitable, auto& timestamp) {
    asio::steady_timer timer(co_await asio::this_coro::executor);
    timer.expires_at(timestamp);
    asio::error_code ec;
    co_await timer.async_wait(asio::redirect_error(asio::use_awaitable, ec));
    if (!ec) {
        co_await std::forward<Awaitable>(awaitable);
    }
}
} // namespace core::timer