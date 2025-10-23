#include "executor.h"
#include <spdlog/spdlog.h>

namespace core {
void Executor::start() {
    if (running_.exchange(true)) {
        spdlog::warn("Executor is already running");
        return;
    }
    if (!work_guard_) {
        work_guard_.emplace(asio::make_work_guard(io_context_));
    }
    spdlog::info("Executor started with {} worker threads", concurrency_);
    io_context_.run();
}

void Executor::stop() {
    if (!running_.exchange(false)) {
        spdlog::warn("Executor is not running");
        return;
    }

    spdlog::info("Stopping Executor...");
    if (work_guard_) {
        work_guard_.reset();
    }

    io_context_.stop();
    thread_pool_.stop();
    thread_pool_.join();
    spdlog::info("Executor stopped");
}

void Executor::restart() {
    spdlog::info("Restarting Executor...");
    stop();

    io_context_.restart();

    spdlog::warn("Thread pool cannot be restarted, creating new instance");
    thread_pool_.~thread_pool();
    new (&thread_pool_) asio::thread_pool(concurrency_);

    start();
}
} // namespace core