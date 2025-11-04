#pragma once

#include <concepts>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

using MutDataBlock = std::span<std::byte>;
using ConstDataBlock = std::span<const std::byte>;

constexpr size_t kDefaultBufferSize = 1024 * 1024 + 512;

namespace util {

template<typename T>
concept ProtobufMessage = requires(T msg, void* ptr, int size) {
    { msg.ByteSizeLong() } -> std::same_as<size_t>;
    { msg.SerializeToArray(ptr, size) } -> std::same_as<bool>;
    { msg.ParseFromArray(ptr, size) } -> std::same_as<bool>;
};

template<ProtobufMessage T>
ConstDataBlock serialize(const T& message, std::vector<std::byte>& buffer) {
    const size_t size = message.ByteSizeLong();
    if (buffer.size() < size) {
        buffer.resize(size);
    }

    if (!message.SerializeToArray(buffer.data(), static_cast<int>(size))) {
        return {};
    }

    return ConstDataBlock(buffer.data(), size);
}

template<ProtobufMessage T>
ConstDataBlock serialize(const T& message, MutDataBlock buffer) {
    const size_t size = message.ByteSizeLong();
    if (buffer.size() < size) {
        return {};
    }

    if (!message.SerializeToArray(buffer.data(), static_cast<int>(size))) {
        return {};
    }

    return ConstDataBlock(buffer.data(), size);
}

template<ProtobufMessage T>
bool deserialize(ConstDataBlock data, T& message) {
    if (data.empty()) {
        return false;
    }
    return message.ParseFromArray(data.data(), static_cast<int>(data.size()));
}

template<ProtobufMessage T>
std::optional<T> deserialize(ConstDataBlock data) {
    if (data.empty()) {
        return std::nullopt;
    }

    T message;
    if (!message.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
        return std::nullopt;
    }

    return message;
}

} // namespace util
