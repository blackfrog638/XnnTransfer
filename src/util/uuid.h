#pragma once

#include <random>
#include <string>
#include <stduuid/uuid.h>

namespace util {

// Generate a random UUID v4 and return as string
inline std::string generate_uuid() {
    thread_local std::random_device rd;
    thread_local std::mt19937 generator(rd());

    uuids::uuid_random_generator gen{generator};
    return uuids::to_string(gen());
}

} // namespace util