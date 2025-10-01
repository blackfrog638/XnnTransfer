#include "executor.h"
#include <spdlog/spdlog.h>

namespace core {
void Executor::start() {
    if (running_.exchange(true)) {
        return;
    }
    if (!work_guard_) {
        work_guard_.emplace(asio::make_work_guard(io_context_));
    }
    io_context_.restart();
    io_context_.run();
    running_.store(false, std::memory_order_release);
    spdlog::info("Executor started. ");
}

void Executor::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (work_guard_) {
        work_guard_.reset();
    }
    io_context_.stop();
    spdlog::info("Executor stopped. ");
}

void Executor::restart() {
    stop();
    start();
}
} // namespace core