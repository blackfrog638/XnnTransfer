#pragma once

#include "core/executor.h"
#include "discovery/online_list_inspector.h"
#include <asio/awaitable.hpp>

namespace cli {

class OnlineListDisplay {
  public:
    explicit OnlineListDisplay(core::Executor& executor, discovery::OnlineListInspector& inspector)
        : executor_(executor)
        , inspector_(inspector)
        , running_(false) {}

    void start();
    void stop();

  private:
    asio::awaitable<void> display_loop();

    void print_header();
    void print_online_list();
    std::string format_timestamp(int64_t timestamp_ms);

    core::Executor& executor_;
    discovery::OnlineListInspector& inspector_;
    bool running_;
    static constexpr int kRefreshIntervalMs = 1000;
};

} // namespace cli
