#include "cli/online_list_display.h"
#include "core/executor.h"
#include "discovery/discovery_handler.h"
#include "util/settings.h"
#include <atomic>
#include <condition_variable>
#include <csignal>
#include <mutex>
#include <spdlog/spdlog.h>
#include <thread>

static cli::OnlineListDisplay* g_display = nullptr;
static core::Executor* g_executor = nullptr;
static std::atomic<bool> g_running{true};
static std::mutex g_mutex;
static std::condition_variable g_cv;

void signal_handler(int signal) {
    if (signal == SIGINT) {
        spdlog::info("\nReceived interrupt signal, shutting down...");
        if (g_display) {
            g_display->stop();
        }
        if (g_executor) {
            g_executor->stop();
        }
        g_running = false;
        g_cv.notify_all();
    }
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("XnnTransfer starting...");

    util::Settings::instance().init(argv[0]);

    auto& settings = util::Settings::instance();
    spdlog::info("Username: {}", settings.get()["username"].get<std::string>());

    core::Executor executor;
    g_executor = &executor;

    spdlog::info("Creating discovery handler...");
    discovery::DiscoveryHandler discovery_handler(executor);
    discovery_handler.start();
    cli::OnlineListDisplay display(executor, discovery_handler.online_list_inspector_);
    g_display = &display;

    std::signal(SIGINT, signal_handler);

    display.start();
    std::thread io_thread([&executor]() { executor.start(); });
    {
        std::unique_lock<std::mutex> lock(g_mutex);
        g_cv.wait(lock, [] { return !g_running.load(); });
    }

    spdlog::info("Shutting down...");
    if (io_thread.joinable()) {
        io_thread.join();
    }
    return 0;
}
