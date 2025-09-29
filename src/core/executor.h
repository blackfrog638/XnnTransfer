#pragma once

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <optional>
#include <thread>

namespace core {
class Executor {
  public:
    Executor() = default;
    ~Executor() = default;

    Executor(const Executor&) = delete;
    Executor& operator=(const Executor&) = delete;

    void start();
    void stop();
    void restart();

    asio::io_context& get_io_context() { return io_context_; }

    template<typename Awaitable>
    auto spawn(Awaitable&& awaitable) {
        return asio::co_spawn(io_context_, std::forward<Awaitable>(awaitable), asio::detached);
    }

  private:
    asio::io_context io_context_;
    std::optional<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_{};
    size_t concurrency_ = std::thread::hardware_concurrency();
    std::atomic<bool> running_{false};
};
} // namespace core