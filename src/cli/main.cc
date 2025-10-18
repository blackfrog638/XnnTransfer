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

// 全局变量用于信号处理
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
    try {
        // 降低日志级别，避免干扰显示
        spdlog::set_level(spdlog::level::debug);
        spdlog::info("XnnTransfer starting...");

        util::Settings::instance().init(argv[0]);

        auto& settings = util::Settings::instance();
        spdlog::info("Username: {}", settings.get()["username"].get<std::string>());

        spdlog::info("Creating executor...");
        core::Executor executor;
        g_executor = &executor;

        spdlog::info("Creating discovery handler...");
        discovery::DiscoveryHandler discovery_handler(executor);
        discovery_handler.start();

        // 创建并启动在线列表显示
        spdlog::info("Creating online list display...");
        cli::OnlineListDisplay display(executor, discovery_handler.online_list_inspector_);
        g_display = &display;

        // 注册信号处理
        std::signal(SIGINT, signal_handler);

        // 启动显示和服务
        spdlog::info("Starting display...");
        display.start();

        // 在单独的线程中运行 executor
        spdlog::info("Starting executor thread...");
        std::thread executor_thread([&executor]() {
            try {
                spdlog::info("Executor thread started");
                executor.start();
                spdlog::info("Executor stopped normally");
            } catch (const std::exception& e) {
                spdlog::error("Executor thread exception: {}", e.what());
            }
        });

        // 主线程等待信号
        spdlog::info("Main thread waiting for signal...");
        {
            std::unique_lock<std::mutex> lock(g_mutex);
            g_cv.wait(lock, [] { return !g_running.load(); });
        }

        spdlog::info("Shutting down gracefully...");

        // 等待 executor 线程结束
        if (executor_thread.joinable()) {
            spdlog::info("Joining executor thread...");
            executor_thread.join();
        }

        spdlog::info("XnnTransfer stopped");
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("Fatal error in main: {}", e.what());
        return 1;
    } catch (...) {
        spdlog::error("Unknown fatal error in main");
        return 1;
    }
}
