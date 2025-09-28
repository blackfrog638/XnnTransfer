#include "executor.h"

namespace core {
void Executor::start() {
    if (running_.exchange(true)) {
        return;
    }
    io_context_.run();
}

void Executor::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    io_context_.stop();
}

void Executor::restart() {
    stop();
    start();
}
} // namespace core