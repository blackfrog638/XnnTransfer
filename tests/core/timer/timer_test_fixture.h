#pragma once

#include "gtest/gtest.h"
#include <core/executor.h>
#include <thread>

class TimerTest : public ::testing::Test {
  protected:
    core::Executor executor;
    std::thread runner;

    void SetUp() override {
        runner = std::thread([this]() { executor.start(); });
    }

    void TearDown() override {
        executor.stop();
        if (runner.joinable()) {
            runner.join();
        }
    }
};
