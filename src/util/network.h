#pragma once

#include <string>

namespace asio {
class io_context;
}

namespace util {

std::string get_local_ip(asio::io_context& io_context);

} // namespace util
