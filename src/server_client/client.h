#ifndef CLIENT_H
#define CLIENT_H


// client.h

#pragma once

#include <QObject>
#include <QString>
#include <boost/asio.hpp>

class Client : public QObject
{
    Q_OBJECT

public:
    explicit Client(QObject* parent = nullptr);
    ~Client();

    void connectToServer(const QString& ip, unsigned short port);

signals:
    void connectedToServer();                 // 连接成功
    void connectionFailed(const QString&);   // 连接失败
    void receivedMessage(const QString&);    // 收到服务端消息

private:
    void doConnect(const std::string& ip, unsigned short port);
    void doRead();

    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
    std::unique_ptr<boost::asio::ip::tcp::resolver> resolver_;
    std::thread networkThread_;
};


#endif // CLIENT_H
