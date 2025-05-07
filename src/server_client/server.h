#ifndef SEVER_H
#define SEVER_H

#pragma once

#include <QObject>
#include <memory>
#include <thread>
#include <boost/asio.hpp>

class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(QObject* parent = nullptr);
    ~Server();

    void startServer(unsigned short port); // 启动服务器
    void stopServer(); // 停止服务器

signals:
    void newSessionConnected(const QString& clientInfo); // 有新连接时发出信号

private:
    void doAccept(); // 接收连接

    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::thread networkThread_;
    bool running_ = false;
};


#endif // SEVER_H
