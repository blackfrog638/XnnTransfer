#pragma once

#include "core/executor.h"
#include "discovery/heartbeat.h"
#include "discovery/online_list_inspector.h"
#include "heartbeat.h"

namespace discovery {
class DiscoveryHandler {
  public:
    explicit DiscoveryHandler(core::Executor& executor)
        : executor_(executor)
        , heartbeat_(executor)
        , online_list_inspector_(executor) {}
    ~DiscoveryHandler() = default;

    DiscoveryHandler(const DiscoveryHandler&) = delete;
    DiscoveryHandler& operator=(const DiscoveryHandler&) = delete;

    Heartbeat heartbeat_;
    OnlineListInspector online_list_inspector_;

    void start() {
        executor_.spawn(heartbeat_.start());
        online_list_inspector_.start();
    }

    void stop() {
        executor_.spawn(heartbeat_.stop());
        online_list_inspector_.stop();
    }

    void restart() {
        executor_.spawn(heartbeat_.restart());
        online_list_inspector_.restart();
    }

  private:
    core::Executor& executor_;
};
} // namespace discovery