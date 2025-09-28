# 项目结构

项目分成以下模块：
- `core`: 核心层
	- **Executor**：封装 `asio::io_context`/`thread_pool` 并以协程形式启动、停止与调度全局任务。
	- **Connector / Acceptor**：提供 `asio::awaitable<>` 接口的拨号与监听器，负责建立客户端与服务端的基础连接。
	- **Utilities**：协程友好的定时器、重连、广播等辅助工具，供上层模块复用。
	核心层聚焦 asio 的具体细节，确保上层只与领域逻辑交互，而无需关注底层 I/O。
- `components` 组件层
- `util`: 相关基础设施
- `cli`:  命令行工具
- `discovery`: 专门用于设备的发现（因为与下述`server`的业务关系较为独立）
- `server`: 项目接收、发送请求、处理会话主要逻辑

