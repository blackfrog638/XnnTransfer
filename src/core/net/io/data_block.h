#pragma once

#include <cstddef>
#include <span>

namespace core::net::io {
using MutDataBlock = std::span<std::byte>;
using ConstDataBlock = std::span<const std::byte>;
} // namespace core::net::io
