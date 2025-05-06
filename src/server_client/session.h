#ifndef SESSION_H
#define SESSION_H


#pragma once

#include <QObject>
#include <boost/asio.hpp>

class Session : public QObject, public std::enable_shared_from_this<Session>
{
    Q_OBJECT
public:
    explicit Session(boost::asio::ip::tcp::socket socket, QObject* parent = nullptr);

    void start(); // 开始会话

signals:
    void sessionStarted(const QString& clientInfo); // 通知有新连接

private:
    void doWrite(); //可以做提示发送给客户端输入密码
    void doRead();  // 读取客户端信息
    boost::asio::ip::tcp::socket socket_;
    boost::asio::streambuf read_buffer_; // 用于接收数据

};

#endif // SESSION_H
