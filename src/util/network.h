#pragma once

#include <string>

namespace asio {
class io_context;
}

namespace util {

// Get local IP address using ASIO resolver
// Returns empty string if failed to get local IP
std::string get_local_ip(asio::io_context& io_context);

} // namespace util
