#pragma once
#include <QMainWindow>
#include <QTimer>
#include <QInputDialog>
#include <atomic>
#include <mutex>
#include <thread>
#include <boost/asio.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/* ---- C++11 兼容 make_unique ---- */
#if __cplusplus < 201402L
namespace std { template<class T, class... A>
    std::unique_ptr<T> make_unique(A&&... a)
    { return std::unique_ptr<T>(new T(std::forward<A>(a)...)); } }
#endif
/* -------------------------------- */

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /* ===== 对外接口 ===== */
    void setPeer(const QString& ip, quint16 port);        // 修改目标地址
    void startFileSend(const QString& filePath);          // 触发发送文件

private slots:
    void onSendClicked();
    void onCheckRecv();

private:
    /* ===== 网络流程 ===== */
    void startAccept();
    void asyncRead();
    void connectToPeer();
    void scheduleReconnect();
    void safeCloseSocket(const QString&);

    /* ===== 认证流程 ===== */
    void startAuth();                                     // 发送我方口令
    void onAuthMessage(const QString& msg);               // 处理对方口令
    bool verifyPassword(const QString& pwd);              // TODO: 改成安全验证
    void onAuthPassed();                                  // 认证通过后调用

    /* ===== 文件协议占位 ===== */
    void handleFilePacket(const QByteArray& pkt);         // 收到文件数据

    /* ===== 状态 ===== */
    Ui::MainWindow *ui{nullptr};

    QString peerIp   = "192.168.31.68";
    quint16 peerPort = 9001;
    static constexpr quint16 listenPort = 9000;

    bool connected  {false};
    bool connecting {false};
    bool authed     {false};                              // 口令是否通过

    boost::asio::io_service io_service;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor;
    std::unique_ptr<boost::asio::ip::tcp::socket>   socket;
    std::thread io_thread;
    std::atomic<bool> running{true};

    std::mutex  recv_mutex;
    std::string recv_buffer;
};
