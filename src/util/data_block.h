#pragma once

#include <concepts>
#include <cstddef>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <vector>

using MutDataBlock = std::span<std::byte>;
using ConstDataBlock = std::span<const std::byte>;

namespace util {
template<typename T>
concept ProtobufMessage = requires(T msg, const std::string& str) {
    { msg.SerializeAsString() } -> std::same_as<std::string>;
    { msg.ParseFromString(str) } -> std::same_as<bool>;
};

template<ProtobufMessage T>
std::vector<std::byte> serialize_message(const T& message) {
    std::string serialized = message.SerializeAsString();
    std::vector<std::byte> buffer(serialized.size());
    std::memcpy(buffer.data(), serialized.data(), serialized.size());
    return buffer;
}

template<ProtobufMessage T>
std::optional<T> deserialize_message(std::span<const std::byte> data) {
    if (data.empty()) {
        return std::nullopt;
    }

    T message;
    std::string serialized(reinterpret_cast<const char*>(data.data()), data.size());
    if (!message.ParseFromString(serialized)) {
        return std::nullopt;
    }

    return message;
}

} // namespace util
