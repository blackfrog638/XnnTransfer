#include "server.h"
#include "session.h"
#include <QDebug>
using boost::asio::ip::tcp;

Server::Server(QObject* parent)
    : QObject(parent)
{}

Server::~Server()
{
    stopServer();
}

void Server::startServer(unsigned short port)
{    qDebug() << "传输端口：" << port;

    acceptor_ = std::make_unique<tcp::acceptor>(io_context_, tcp::endpoint(tcp::v4(), port));

    running_ = true;

    doAccept();

    networkThread_ = std::thread([this]() {
        io_context_.run();
    });
}

void Server::stopServer()
{
    if (running_) {
        io_context_.stop();
        if (networkThread_.joinable()) {
            networkThread_.join();
        }
        running_ = false;
    }
}

void Server::doAccept()
{
    acceptor_->async_accept(
                //
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                // 创建 Session，连接信号
                auto session = std::make_shared<Session>(std::move(socket));
                connect(session.get(), &Session::sessionStarted, this, &Server::newSessionConnected);
                session->start();
            }

            if (running_) {
                doAccept(); //
            }
        });
}
