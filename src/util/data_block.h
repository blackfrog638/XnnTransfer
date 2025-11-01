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

// Concept to check if a type is a protobuf message
template<typename T>
concept ProtobufMessage = requires(T msg, const std::string& str) {
    { msg.SerializeAsString() } -> std::same_as<std::string>;
    { msg.ParseFromString(str) } -> std::same_as<bool>;
};

// Serialize a protobuf message to a byte vector
template<ProtobufMessage T>
std::vector<std::byte> serialize_message(const T& message) {
    std::string serialized = message.SerializeAsString();
    std::vector<std::byte> buffer(serialized.size());
    std::memcpy(buffer.data(), serialized.data(), serialized.size());
    return buffer;
}

// Deserialize a byte span to a protobuf message
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
