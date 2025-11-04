# TcpInteractor 迁移指南

## 概述

新的 `TcpInteractor` 统一了 `TcpSender` 和 `TcpReceiver` 的功能，支持双向通信。

## 核心变化

### 旧架构
```cpp
// 发送方
TcpSender sender(executor, socket, "127.0.0.1", port);
sender.start_connect();

// 接收方
TcpReceiver receiver(executor, socket, port);
receiver.start_accept();
```

### 新架构
```cpp
// 客户端模式（主动连接）
TcpInteractor client(executor, socket, "127.0.0.1", port);
client.start();

// 服务端模式（监听接受）
TcpInteractor server(executor, socket, port);
server.start();
```

## 优势

1. **双向通信**：一个 `TcpInteractor` 既可以发送也可以接收
2. **统一接口**：无需区分 Sender/Receiver，API 完全一致
3. **模式自动识别**：根据构造函数自动确定客户端/服务端模式
4. **完整 Protobuf 支持**：内置 `send_message`/`receive_message` 模板方法

## 迁移步骤

### 1. Session 类迁移

#### sender::Session

**旧代码：**
```cpp
class Session {
private:
    asio::ip::tcp::socket socket_;
    core::net::io::TcpSender tcp_sender_;
};

Session::Session(core::Executor& executor, std::string_view destination, ...)
    : socket_(executor.get_io_context())
    , tcp_sender_(executor, socket_, destination, port)
{ 
    // ...
}
```

**新代码：**
```cpp
class Session {
private:
    asio::ip::tcp::socket socket_;
    core::net::io::TcpInteractor tcp_interactor_;
};

Session::Session(core::Executor& executor, std::string_view destination, ...)
    : socket_(executor.get_io_context())
    , tcp_interactor_(executor, socket_, destination, port)  // 客户端模式
{ 
    tcp_interactor_.start();
    // ...
}
```

#### receiver::Session

**旧代码：**
```cpp
class Session {
private:
    asio::ip::tcp::socket socket_;
    core::net::io::TcpReceiver tcp_receiver_;
};

Session::Session(core::Executor& executor)
    : socket_(executor.get_io_context())
    , tcp_receiver_(executor, socket_, kListeningPort)
{ 
    // ...
}
```

**新代码：**
```cpp
class Session {
private:
    asio::ip::tcp::socket socket_;
    core::net::io::TcpInteractor tcp_interactor_;
};

Session::Session(core::Executor& executor)
    : socket_(executor.get_io_context())
    , tcp_interactor_(executor, socket_, kListeningPort)  // 服务端模式
{ 
    tcp_interactor_.start();
    // ...
}
```

### 2. SingleFileSender 迁移

**旧代码：**
```cpp
class SingleFileSender {
public:
    SingleFileSender(core::Executor& executor,
                     core::net::io::TcpSender& tcp_sender,
                     ...);
private:
    core::net::io::TcpSender& tcp_sender_;
};
```

**新代码：**
```cpp
class SingleFileSender {
public:
    SingleFileSender(core::Executor& executor,
                     core::net::io::TcpInteractor& tcp_interactor,
                     ...);
private:
    core::net::io::TcpInteractor& tcp_interactor_;
};
```

### 3. SingleFileReceiver 迁移

**旧代码：**
```cpp
class SingleFileReceiver {
public:
    SingleFileReceiver(core::Executor& executor,
                       core::net::io::TcpReceiver& tcp_receiver);
private:
    core::net::io::TcpReceiver& tcp_receiver_;
};
```

**新代码：**
```cpp
class SingleFileReceiver {
public:
    SingleFileReceiver(core::Executor& executor,
                       core::net::io::TcpInteractor& tcp_interactor);
private:
    core::net::io::TcpInteractor& tcp_interactor_;
};
```

### 4. 使用新的 send_message/receive_message

**旧代码（需要手动序列化）：**
```cpp
transfer::FileChunkRequest chunk;
chunk.set_session_id(session_id);
chunk.set_chunk_index(index);
// ...

std::string payload;
chunk.SerializeToString(&payload);
ConstDataBlock data(reinterpret_cast<const std::byte*>(payload.data()), payload.size());
co_await tcp_sender_.send(data);
```

**新代码（直接发送 protobuf）：**
```cpp
transfer::FileChunkRequest chunk;
chunk.set_session_id(session_id);
chunk.set_chunk_index(index);
// ...

co_await tcp_interactor_.send_message(chunk);
```

**接收消息也变得更简单：**
```cpp
// 旧代码
std::vector<std::byte> buffer(1024 * 1024);
MutDataBlock buf_span(buffer.data(), buffer.size());
co_await tcp_receiver_.receive(buf_span);

transfer::FileChunkResponse response;
std::string data(reinterpret_cast<const char*>(buf_span.data()), buf_span.size());
response.ParseFromString(data);

// 新代码
auto response = co_await tcp_interactor_.receive_message<transfer::FileChunkResponse>();
if (response) {
    // 使用 response.value()
}
```

## API 对照表

| 旧 API | 新 API | 说明 |
|--------|--------|------|
| `TcpSender::start_connect()` | `TcpInteractor::start()` | 统一启动方法 |
| `TcpReceiver::start_accept()` | `TcpInteractor::start()` | 统一启动方法 |
| `tcp_sender_.send(data)` | `tcp_interactor_.send(data)` | 相同 |
| `tcp_receiver_.receive(buffer)` | `tcp_interactor_.receive(buffer)` | 相同 |
| `tcp_sender_.send_message(msg)` | `tcp_interactor_.send_message(msg)` | 相同 |
| `tcp_receiver_.receive_message<T>()` | `tcp_interactor_.receive_message<T>()` | 相同 |
| N/A | `tcp_interactor_.mode()` | 查询客户端/服务端模式 |
| N/A | `tcp_interactor_.is_connected()` | 检查连接状态 |

## 完整示例

### 简单的客户端-服务端通信

```cpp
#include "core/executor.h"
#include "core/net/io/tcp_interactor.h"
#include "transfer.pb.h"

void example() {
    core::Executor executor;
    
    // 服务端
    asio::ip::tcp::socket server_socket(executor.get_io_context());
    core::net::io::TcpInteractor server(executor, server_socket, 14648);
    server.start();
    
    // 客户端
    asio::ip::tcp::socket client_socket(executor.get_io_context());
    core::net::io::TcpInteractor client(executor, client_socket, "127.0.0.1", 14648);
    client.start();
    
    // 客户端发送 protobuf 消息
    executor.spawn([&]() -> asio::awaitable<void> {
        transfer::FileInfoRequest request;
        request.set_relative_path("test.txt");
        request.set_size(1024);
        
        co_await client.send_message(request);
        
        auto response = co_await client.receive_message<transfer::FileInfoResponse>();
        if (response) {
            // 处理响应
        }
    });
    
    // 服务端接收并响应
    executor.spawn([&]() -> asio::awaitable<void> {
        auto request = co_await server.receive_message<transfer::FileInfoRequest>();
        if (request) {
            transfer::FileInfoResponse response;
            response.set_status(transfer::FileInfoResponse::Status::FileInfoResponse_Status_READY);
            co_await server.send_message(response);
        }
    });
    
    executor.run();
}
```

## 注意事项

1. **记得调用 `start()`**：创建 `TcpInteractor` 后必须调用 `start()` 方法
2. **构造函数区分模式**：
   - 3参数构造函数（executor, socket, port）→ 服务端模式
   - 4参数构造函数（executor, socket, host, port）→ 客户端模式
3. **删除 make_block 辅助函数**：不再需要手动包装 protobuf 消息
4. **统一错误处理**：`receive_message` 返回 `std::optional<T>`，检查是否有值

## 删除的文件

迁移完成后可以删除：
- `src/core/net/io/tcp_sender.h`
- `src/core/net/io/tcp_sender.cc`
- `src/core/net/io/tcp_receiver.h`
- `src/core/net/io/tcp_receiver.cc`
- `tests/core/net/io/tcp_tests.cc`（替换为 `tcp_interactor_tests.cc`）
