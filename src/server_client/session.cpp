

#include "session.h"
#include <QString>
#include <boost/asio/write.hpp>

using boost::asio::ip::tcp;

Session::Session(tcp::socket socket, QObject* parent)
    : QObject(parent),
      socket_(std::move(socket))
{}

void Session::start()
{
    QString clientInfo = QString::fromStdString(socket_.remote_endpoint().address().to_string() + ":" + std::to_string(socket_.remote_endpoint().port()));
    emit sessionStarted(clientInfo); // 发出信号 设计ui更新，暂时未写

    doWrite();
}
//简单版，无密码验证，连接建立后给客户端发送消息
void Session::doWrite()
{
    auto self(shared_from_this());//防止对象过早销毁
    std::string message = "欢迎使用ByteSpark传输软件！请输入password：\n";

    boost::asio::async_write(socket_, boost::asio::buffer(message),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                // 这里做接收客户端数据接口，
                doRead();
                //
            }
        });
}
//读取客户端信息
void Session::doRead(){




}
