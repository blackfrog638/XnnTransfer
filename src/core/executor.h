#pragma once

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/thread_pool.hpp>
#include <optional>
#include <thread>

namespace core {
class Executor {
  public:
    Executor()
        : Executor(std::thread::hardware_concurrency()) {}

    explicit Executor(size_t thread_count)
        : concurrency_(thread_count > 0 ? thread_count : 1)
        , thread_pool_(concurrency_)
        , io_context_()
        , work_guard_() {}

    ~Executor() {
        if (running_.load()) {
            stop();
        }
    }

    Executor(const Executor&) = delete;
    Executor& operator=(const Executor&) = delete;

    void start();
    void stop();
    void restart();

    asio::io_context& get_io_context() { return io_context_; }
    asio::thread_pool& get_thread_pool() { return thread_pool_; }
    size_t get_thread_count() const { return concurrency_; }

    enum class Context { IO, ThreadPool };

    template<typename Awaitable>
    auto spawn(Awaitable&& awaitable, Context ctx = Context::IO) {
        if (ctx == Context::IO) {
            return asio::co_spawn(io_context_, std::forward<Awaitable>(awaitable), asio::detached);
        } else {
            return asio::co_spawn(thread_pool_, std::forward<Awaitable>(awaitable), asio::detached);
        }
    }

    template<typename Awaitable, typename CompletionToken>
    auto spawn(Awaitable&& awaitable, CompletionToken&& token, Context ctx = Context::IO) {
        if (ctx == Context::IO) {
            return asio::co_spawn(io_context_,
                                  std::forward<Awaitable>(awaitable),
                                  std::forward<CompletionToken>(token));
        } else {
            return asio::co_spawn(thread_pool_,
                                  std::forward<Awaitable>(awaitable),
                                  std::forward<CompletionToken>(token));
        }
    }

  private:
    size_t concurrency_;
    asio::thread_pool thread_pool_;
    asio::io_context io_context_;
    std::optional<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_{};
    std::atomic<bool> running_{false};
};
} // namespace core