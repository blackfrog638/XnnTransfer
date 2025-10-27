#pragma once

#include <cstddef>
#include <span>

using MutDataBlock = std::span<std::byte>;
using ConstDataBlock = std::span<const std::byte>;
