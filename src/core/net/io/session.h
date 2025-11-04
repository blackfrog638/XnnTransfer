#pragma once
#include "asio/awaitable.hpp"
#include "asio/ip/tcp.hpp"
#include "core/executor.h"
#include "core/net/io/tcp_interactor.h"
#include "session.pb.h"
#include "tcp_interactor.h"
#include <atomic>
#include <cstddef>
#include <vector>

namespace core::net::io {
class Session {
  public:
    Session(core::Executor& executor, std::string_view host, uint16_t port);
    Session(core::Executor& executor, uint16_t port);
    ~Session() = default;

    virtual asio::awaitable<void> start();

    template<typename ProtobufType>
    asio::awaitable<void> send(ProtobufType& message) {
        MessageWrapper wrapper;
        wrapper.set_type(ProtobufType::descriptor()->full_name());

        const size_t message_size = message.ByteSizeLong();
        if (send_buffer_.size() < message_size) {
            send_buffer_.resize(message_size);
        }
        if (!message.SerializeToArray(send_buffer_.data(), static_cast<int>(message_size))) {
            co_return;
        }
        wrapper.set_payload(send_buffer_.data(), message_size);

        co_await interactor_.send_message(wrapper);
    }

  protected:
    core::Executor& executor_;

  private:
    asio::awaitable<void> receive_loop();
    virtual asio::awaitable<void> handle_message(const MessageWrapper& message) = 0;

    std::atomic<bool> running_{false};
    asio::ip::tcp::socket socket_;

    core::net::io::TcpInteractor interactor_;

    std::vector<std::byte> send_buffer_;
};
} // namespace core::net::io