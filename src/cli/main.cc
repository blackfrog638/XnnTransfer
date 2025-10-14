#include "util/settings.h"
#include <spdlog/spdlog.h>

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("XnnTransfer starting...");

    util::Settings::instance().init(argv[0]);

    auto& settings = util::Settings::instance();
    spdlog::info("Username: {}", settings.get()["username"].get<std::string>());

    // 示例：修改并保存配置
    // settings.get()["username"] = "new_user";
    // settings.save();

    return 0;
}
