
#include "core/executor.h"
#include "core/io/tcp_receiver.h"
#include "core/io/tcp_sender.h"
#include "gtest/gtest.h"
#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>

namespace {
TEST(TcpIoTest, SenderAndReceiverExchange) {
    core::Executor executor;
    asio::ip::tcp::socket receiver_socket(executor.get_io_context());
    asio::ip::tcp::socket sender_socket(executor.get_io_context());
    const std::uint16_t kPort = 40201;
    core::net::io::TcpReceiver receiver(executor, receiver_socket, kPort);
    core::net::io::TcpSender sender(executor, sender_socket, "127.0.0.1", kPort);
}
} // namespace