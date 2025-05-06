#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <fstream>

using boost::asio::ip::tcp;

/*------------------------------------------------------------
 | 构造函数：初始化 UI、Boost.Asio、定时器，并启动首次重连
 *-----------------------------------------------------------*/
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 创建监听 socket（被动接收）
    socket   = std::make_unique<tcp::socket>(io_service);
    acceptor = std::make_unique<tcp::acceptor>(
                   io_service, tcp::endpoint(tcp::v4(), listenPort));
    ui->textEdit->append(QString("Listening on %1").arg(listenPort));

    /* 异步等待外部客户端连接 */
    startAccept();

    /* 独立线程驱动 io_service 事件循环 */
    io_thread = std::thread([this]{ io_service.run(); });

    /* 定时将后台接收缓冲刷新到 QTextEdit，避免跨线程 UI 操作 */
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::onCheckRecv);
    timer->start(100);

    /* 点击 Send 按钮时发送文本消息 */
    connect(ui->sendButton, &QPushButton::clicked,
            this, &MainWindow::onSendClicked);

    /* 主动拨号循环：每 2 s 尝试一次，直到连上为止 */
    scheduleReconnect();
}

/*------------------------------------------------------------
 | 析构：安全关闭 socket、停止 io_service、回收线程
 *-----------------------------------------------------------*/
MainWindow::~MainWindow()
{
    running = false;
    safeCloseSocket("Application exit");
    io_service.stop();
    if (io_thread.joinable()) io_thread.join();
    delete ui;
}

/*------------------------------------------------------------
 | setPeer：动态修改目标服务器地址并立即重连
 * ip/port 可来源于 UI 配置界面或其他模块
 *-----------------------------------------------------------*/
void MainWindow::setPeer(const QString& ip, quint16 port)
{
    peerIp   = ip;
    peerPort = port;
    ui->textEdit->append(
        QString("[Config] Peer set to %1:%2").arg(ip).arg(port));

    scheduleReconnect();  // 触发新一轮拨号
}

/*------------------------------------------------------------
 | startFileSend：占位函数，认证通过后才可调用
 | 真实实现应：打开文件 → 分片 → 自定义协议头 → write()
 *-----------------------------------------------------------*/
void MainWindow::startFileSend(const QString& filePath)
{
    if (!authed) {
        ui->textEdit->append("[Warn] Not authenticated, cannot send file.");
        return;
    }
    ui->textEdit->append("[TODO] File send not implemented: " + filePath);
}

/*------------------------------------------------------------
 | connectToPeer：发起一次 async_connect
 | 1) 成功：connected=true，开始读，进入口令认证
 | 2) 失败：2 秒后 scheduleReconnect() 再尝试
 *-----------------------------------------------------------*/
void MainWindow::connectToPeer()
{
    if (connected || connecting) return;
    connecting = true;

    // 若旧 socket 已关闭或不存在，重新分配
    if (!socket || !socket->is_open())
        socket.reset(new tcp::socket(io_service));

    auto resolver = std::make_shared<tcp::resolver>(io_service);
    tcp::resolver::query q(peerIp.toStdString(), std::to_string(peerPort));
    tcp::resolver::iterator it = resolver->resolve(q);

    boost::asio::async_connect(*socket, it,
        [this](const boost::system::error_code& ec, tcp::resolver::iterator)
    {
        connecting = false;
        if (!ec) {
            connected = true;
            authed = false;            // 每次新连接都要重新认证
            ui->textEdit->append(
                QString("[System] Connected to %1:%2").arg(peerIp).arg(peerPort));

            asyncRead();               // 挂异步读
            startAuth();               // 发送我方口令
        } else {
            ui->textEdit->append("[Warn] Connect failed: "
                                 + QString::fromStdString(ec.message()));
            scheduleReconnect();
        }
    });
}

/*------------------------------------------------------------
 | scheduleReconnect：若未连接/未在连，则 2 s 后调用 connectToPeer
 | 该函数既用于首次拨号，也用于断线重连
 *-----------------------------------------------------------*/
void MainWindow::scheduleReconnect()
{
    if (connected || connecting) return;

    auto t = std::make_shared<boost::asio::deadline_timer>(
                 io_service, boost::posix_time::seconds(2));

    t->async_wait([this, t](const boost::system::error_code&)
    {
        if (!running) return;
        if (connected || connecting) return;
        connectToPeer();
    });
}

/*------------------------------------------------------------
 | startAuth：弹出输入框收集口令并发送给对端
 | 简易 Demo，真实环境下请使用加密信道 / HMAC / 零知识证明等
 *-----------------------------------------------------------*/
void MainWindow::startAuth()
{
    bool ok;
    QString pwd = QInputDialog::getText(this, "Password",
                    "Enter password:", QLineEdit::Password, {}, &ok);
    if (!ok) {                           // 用户取消
        safeCloseSocket("Auth cancelled");
        return;
    }
    std::string packet = "PWD:" + pwd.toStdString() + '\n';
    boost::asio::write(*socket, boost::asio::buffer(packet));
}

/*------------------------------------------------------------
 | verifyPassword：本端校验对方口令
 | TODO：接入数据库或哈希比对
 *-----------------------------------------------------------*/
bool MainWindow::verifyPassword(const QString& pwd)
{
    return (pwd == "123456");            // Demo：口令为 123456
}

/*------------------------------------------------------------
 | onAuthMessage：处理认证相关指令
 | 1) 收到 "PWD:xxx" → 校验 → 回 "OK"/"FAIL"
 | 2) 收到 "OK"/"FAIL" → 更新 authed 标志
 *-----------------------------------------------------------*/
void MainWindow::onAuthMessage(const QString& msg)
{
    if (msg.startsWith("PWD:")) {
        QString pwd = msg.mid(4);
        bool ok = verifyPassword(pwd);
        std::string resp = ok ? "OK\n" : "FAIL\n";
        boost::asio::write(*socket, boost::asio::buffer(resp));
        if (ok) {
            authed = true;
            ui->textEdit->append("[System] Peer authenticated.");
            onAuthPassed();
        } else {
            ui->textEdit->append("[Warn] Peer password wrong.");
        }
    } else if (msg == "OK") {            // 我方通过认证
        authed = true;
        ui->textEdit->append("[System] Authenticated by peer.");
        onAuthPassed();
    } else if (msg == "FAIL") {          // 对方口令错误
        ui->textEdit->append("[Warn] Password wrong, please retry.");
        startAuth();                     // 重新输入
    }
}

/*------------------------------------------------------------
 | onAuthPassed：口令双向通过后调用，可开始文件协商
 *-----------------------------------------------------------*/
void MainWindow::onAuthPassed()
{
    ui->textEdit->append("[TODO] Ready for file transfer...");
}

/*------------------------------------------------------------
 | startAccept：异步等待外部设备连上本端 listenPort
 | 若已有连接，额外连接会被拒绝（只保留一条）
 *-----------------------------------------------------------*/
void MainWindow::startAccept()
{
    auto newSock = std::make_shared<tcp::socket>(io_service);
    acceptor->async_accept(*newSock,
        [this, newSock](const boost::system::error_code& ec)
    {
        if (!ec) {
            if (!socket || !socket->is_open()) {
                socket = std::make_unique<tcp::socket>(std::move(*newSock));
                connected = true; authed = false;
                ui->textEdit->append("[System] Incoming connection accepted.");
                asyncRead();
            } else newSock->close();
        }
        if (running) startAccept();       // 继续下一轮 accept
    });
}

/*------------------------------------------------------------
 | asyncRead：持续挂 read_until('\n')
 | 未认证阶段只处理认证指令；认证后再写入聊天缓冲或文件协议
 *-----------------------------------------------------------*/
void MainWindow::asyncRead()
{
    auto buf = std::make_shared<boost::asio::streambuf>();
    boost::asio::async_read_until(*socket, *buf, '\n',
        [this, buf](const boost::system::error_code& ec, std::size_t)
    {
        if (!ec) {
            std::istream is(buf.get());
            std::string line; std::getline(is, line);
            QString qline = QString::fromStdString(line);

            if (!authed) {
                onAuthMessage(qline);
            } else if (qline.startsWith("FILE:")) {
                handleFilePacket(QByteArray::fromStdString(line));
            } else {
                { std::lock_guard<std::mutex> lk(recv_mutex);
                  recv_buffer += line + '\n'; }
            }
            asyncRead();  // 继续读取
        } else {
            safeCloseSocket("Disconnected: " +
                            QString::fromStdString(ec.message()));
            scheduleReconnect();
        }
    });
}

/*------------------------------------------------------------
 | handleFilePacket：文件协议占位
 | 解析自定义头 / 块号 / 数据，并写入磁盘或缓存
 *-----------------------------------------------------------*/
void MainWindow::handleFilePacket(const QByteArray& pkt)
{
    ui->textEdit->append(
        "[TODO] Received file packet len=" + QString::number(pkt.size()));
}

/*------------------------------------------------------------
 | safeCloseSocket：统一关闭、清状态、提示 UI
 *-----------------------------------------------------------*/
void MainWindow::safeCloseSocket(const QString& reason)
{
    if (socket && socket->is_open()) {
        boost::system::error_code ignored;
        socket->shutdown(tcp::socket::shutdown_both, ignored);
        socket->close(ignored);
    }
    connected = connecting = authed = false;
    ui->textEdit->append("[System] " + reason);
}

/*------------------------------------------------------------
 | onSendClicked：聊天文本发送槽
 | 仅在通过认证后允许发送
 *-----------------------------------------------------------*/
void MainWindow::onSendClicked()
{
    if (!authed) {
        ui->textEdit->append("[Warn] Not authenticated.");
        return;
    }
    QString msg = ui->lineEdit->text();
    if (msg.isEmpty()) return;

    std::string wire = msg.toStdString() + '\n';
    try {
        boost::asio::write(*socket, boost::asio::buffer(wire));
        ui->textEdit->append("[You] " + msg);
        ui->lineEdit->clear();
    } catch (const boost::system::system_error& e) {
        safeCloseSocket("Write failed: " + QString::fromStdString(e.what()));
        scheduleReconnect();
    }
}

/*------------------------------------------------------------
 | onCheckRecv：主线程定时刷新聊天内容到 QTextEdit
 *-----------------------------------------------------------*/
void MainWindow::onCheckRecv()
{
    std::lock_guard<std::mutex> lk(recv_mutex);
    if (!recv_buffer.empty()) {
        ui->textEdit->append("[Peer] " +
            QString::fromStdString(recv_buffer));
        recv_buffer.clear();
    }
}
