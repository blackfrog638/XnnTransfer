# 项目结构

项目分成以下模块：
- `core`: 核心层
	- `executor`：封装 `asio::io_context`/`thread_pool` 并以协程形式启动、停止与调度全局任务。
	- `net`：利用executor封装各种网络相关异步操作。如建立连接、发送/接收UDP/TCP请求。
		- `io`：提供tcp/udp的IO。对于不同协议提供的参数不同。
	- `timer`：时间相关的协程工具，包含定时器与超时功能。
	核心层聚焦 asio 的具体细节，确保上层只与领域逻辑交互，而无需关注底层 I/O。
- `util`: 相关基础设施
- `cli`:  命令行工具
- `discovery`: 专门用于设备的发现（因为与下述`server`的业务关系较为独立）
- `sender`: 文件发送模块
- `receiver`：文件接收模块

