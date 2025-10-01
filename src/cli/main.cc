#include <fmt/format.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

int main(int argc, char** argv) {
    std::cout << "hello world!" << std::endl;
    return 0;
}
