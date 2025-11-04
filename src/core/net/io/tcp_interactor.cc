#include "tcp_interactor.h"
#include <asio/redirect_error.hpp>
#include <asio/steady_timer.hpp>

namespace core::net::io {

// 客户端模式构造函数
TcpInteractor::TcpInteractor(Executor& executor,
                             asio::ip::tcp::socket& socket,
                             std::string_view host,
                             uint16_t port)
    : executor_(executor)
    , socket_(socket)
    , mode_(TcpInteractorMode::Client)
    , connector_(std::in_place, executor, socket)
    , host_(host)
    , port_(port)
    , ready_signal_(std::make_shared<asio::steady_timer>(executor.get_io_context())) {
    ready_signal_->expires_at(asio::steady_timer::time_point::max());
}

// 服务端模式构造函数
TcpInteractor::TcpInteractor(Executor& executor, asio::ip::tcp::socket& socket, std::uint16_t port)
    : executor_(executor)
    , socket_(socket)
    , mode_(TcpInteractorMode::Server)
    , port_(port)
    , acceptor_(std::in_place, executor, socket)
    , ready_signal_(std::make_shared<asio::steady_timer>(executor.get_io_context())) {
    ready_signal_->expires_at(asio::steady_timer::time_point::max());
    if (acceptor_) {
        acceptor_->listen(port_);
    }
}

void TcpInteractor::start() {
    if (mode_ == TcpInteractorMode::Client) {
        // 客户端模式：主动连接
        executor_.spawn([this]() -> asio::awaitable<void> {
            if (!connector_) {
                co_return;
            }
            const bool connected = co_await connector_->connect(host_, port_);
            connected_.store(connected);
            ready_signal_->cancel();
        });
    } else {
        // 服务端模式：监听并接受连接
        executor_.spawn([this]() -> asio::awaitable<void> {
            if (!acceptor_) {
                co_return;
            }
            co_await acceptor_->accept();
            connected_.store(true);
            ready_signal_->cancel();
        });
    }
}

asio::awaitable<void> TcpInteractor::wait_for_ready() {
    if (!connected_.load()) {
        asio::error_code ec;
        co_await ready_signal_->async_wait(asio::redirect_error(asio::use_awaitable, ec));
    }
}

asio::awaitable<void> TcpInteractor::send(ConstDataBlock data) {
    if (data.empty()) {
        co_return;
    }

    co_await wait_for_ready();

    if (!socket_.is_open()) {
        co_return;
    }

    co_await socket_.async_send(asio::buffer(static_cast<const void*>(data.data()), data.size()),
                                asio::use_awaitable);
}

asio::awaitable<void> TcpInteractor::receive(MutDataBlock& buffer) {
    if (buffer.empty()) {
        co_return;
    }

    co_await wait_for_ready();

    if (!socket_.is_open()) {
        co_return;
    }

    std::size_t bytes_received = co_await socket_.async_receive(asio::buffer(static_cast<void*>(
                                                                                 buffer.data()),
                                                                             buffer.size()),
                                                                asio::use_awaitable);
    buffer = buffer.first(bytes_received);
}

} // namespace core::net::io
