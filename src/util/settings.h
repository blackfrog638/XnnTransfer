#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>

using namespace nlohmann;

namespace util {
class Settings {
  public:
    static Settings& instance() {
        static Settings instance;
        return instance;
    }

    void init(const std::string& executable_path = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!file_path_.empty()) {
            spdlog::warn(
                "[Settings::init] Settings already initialized, ignoring new initialization");
            return;
        }

        std::filesystem::path exe_dir;
        if (!executable_path.empty()) {
            exe_dir = std::filesystem::path(executable_path).parent_path();
        } else {
            exe_dir = std::filesystem::current_path();
        }

        file_path_ = (exe_dir / "settings.json").string();
        spdlog::info("[Settings::init] Settings file path: {}", file_path_);

        load();
    }

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    json& get() {
        std::lock_guard<std::mutex> lock(mutex_);
        return settings_;
    }

    const json& get() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return settings_;
    }

    void save() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_path_.empty()) {
            spdlog::error("[Settings::save] Settings not initialized, call init() first");
            return;
        }
        std::ofstream file(file_path_);
        if (!file.is_open()) {
            spdlog::error("[Settings::save] Failed to open config file for writing: {}", file_path_);
            return;
        }
        file << settings_.dump(4);
        spdlog::info("[Settings::save] Settings saved to {}", file_path_);
    }

  private:
    Settings() = default;
    ~Settings() = default;

    void load() {
        if (std::filesystem::exists(file_path_)) {
            std::ifstream file(file_path_);
            if (!file.is_open()) {
                spdlog::error("[Settings::load] Failed to open settings file: {}", file_path_);
                create_default();
                return;
            }
            file >> settings_;
            spdlog::info("[Settings::load] Settings loaded from {}", file_path_);
        } else {
            spdlog::warn("[Settings::load] Settings file not found, creating default: {}",
                         file_path_);
            create_default();
            save_internal();
        }
    }

    void create_default() { settings_ = {{"username", "default_user"}}; }

    void save_internal() {
        std::ofstream file(file_path_);
        if (!file.is_open()) {
            spdlog::error("[Settings::save_internal] Failed to create settings file: {}",
                          file_path_);
            return;
        }
        file << settings_.dump(4);
        spdlog::info("[Settings::save_internal] Default settings created at {}", file_path_);
    }

    std::string file_path_;
    json settings_;
    mutable std::mutex mutex_;
};
} // namespace util