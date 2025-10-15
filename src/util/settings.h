#pragma once

#include <mutex>
#include <nlohmann/json.hpp>
#include <string>


using namespace nlohmann;

namespace util {
class Settings {
  public:
    static Settings& instance() {
        static Settings instance;
        return instance;
    }

    void init(const std::string& executable_path = "");

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

    void save();

  private:
    Settings() = default;
    ~Settings() = default;

    void load();
    void create_default() { settings_ = {{"username", "default_user"}}; }
    void save_internal();

    std::string file_path_;
    json settings_;
    mutable std::mutex mutex_;
};
} // namespace util