#pragma once

#include "util/data_block.h"
#include <array>
#include <filesystem>
#include <optional>
#include <string>

namespace util::hash {
constexpr std::size_t kSha256Size = 32;

std::optional<std::array<std::byte, kSha256Size>> sha256(ConstDataBlock data);
std::optional<std::string> sha256_hex(ConstDataBlock data);

std::optional<std::array<std::byte, kSha256Size>> sha256_file(const std::filesystem::path& file_path);
std::optional<std::string> sha256_file_hex(const std::filesystem::path& file_path);
} // namespace util::hash
