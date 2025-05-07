// client.cpp

#include "client.h"
#include <iostream>
#include <boost/asio/connect.hpp>

using boost::asio::ip::tcp;

Client::Client(QObject* parent)
    : QObject(parent)
{


}

Client::~Client()
{
    io_context_.stop();
    if (networkThread_.joinable()) {
        networkThread_.join();
    }
}

//传入需要连接的ip地址和端口
void Client::connectToServer(const QString& ip, unsigned short port)
{
    std::string ipStr = ip.toStdString();

    // 放到独立线程中执行 io_context
    networkThread_ = std::thread([this, ipStr, port]() {
        doConnect(ipStr, port);
        io_context_.run();
    });
}

void Client::doConnect(const std::string& ip, unsigned short port)
{
    socket_ = std::make_unique<tcp::socket>(io_context_);
    resolver_ = std::make_unique<tcp::resolver>(io_context_);

    auto endpoints = resolver_->resolve(ip, std::to_string(port));
     //此处回调函数调用时机为连接成功。
    boost::asio::async_connect(*socket_, endpoints,
        [this](boost::system::error_code ec, const tcp::endpoint& endpoint) {
            if (!ec) {
                std::cout << "客户端连接成功: " << endpoint << std::endl;
                emit connectedToServer();
                doRead(); // 启动读取服务端数据
            } else {
                QString err = QString::fromStdString(ec.message());
                emit connectionFailed(err);
            }
        });
}

void Client::doRead()
{
    auto buf = std::make_shared<boost::asio::streambuf>();
//此处回调函数调用时机为接收到换行符
    boost::asio::async_read_until(*socket_, *buf, '\n',
        [this, buf](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (!ec) {
                std::istream is(buf.get());
                std::string line;
                std::getline(is, line);
                emit receivedMessage(QString::fromStdString(line));
            } else {
                emit connectionFailed(QString::fromStdString(ec.message()));
            }
        });
}
