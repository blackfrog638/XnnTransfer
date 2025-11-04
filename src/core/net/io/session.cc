#include "core/net/io/session.h"
#include "asio/awaitable.hpp"
namespace core::net::io {
Session::Session(core::Executor& executor, std::string_view host, uint16_t port)
    : socket_(executor.get_io_context())
    , executor_(executor)
    , interactor_(executor_, socket_, host, port) {
    interactor_.start();
    executor_.spawn(start());
}

Session::Session(core::Executor& executor, uint16_t port)
    : socket_(executor.get_io_context())
    , executor_(executor)
    , interactor_(executor_, socket_, port) {
    interactor_.start();
    executor_.spawn(start());
}

asio::awaitable<void> Session::start() {
    running_.store(true);
    executor_.spawn(receive_loop());
    co_return;
}

asio::awaitable<void> Session::receive_loop() {
    while (running_.load()) {
        auto message_opt = co_await interactor_.receive_message<MessageWrapper>();
        if (!message_opt.has_value()) {
            continue;
        }
        co_await handle_message(message_opt.value());
    }
}
} //namespace core::net::io