# XnnTransfer

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![C++](https://img.shields.io/badge/C++-20-blue.svg)](https://en.cppreference.com/)
[![Xmake](https://img.shields.io/badge/XMake-2.x-green.svg)](https://xmake.io/)

## 概述

XnnTransfer 是一个端到端的局域网文件传输工具，使用 C++20 和 XMake 构建。

> 注：本项目得名于谷歌的[XnnPack](https://github.com/google/XNNPACK)，虽然本项目没有使用XnnPack中的任何内容。

## 快速开始

### 环境要求

- 支持 C++20 的编译器
- XMake 构建工具

### 构建和运行

```bash
# 构建项目
xmake

# 运行程序
xmake run
```

## XMake 常用命令

### 基础构建
```bash
xmake                    # 构建项目
xmake f -m debug        # Debug 模式
xmake clean             # 清理项目
xmake f -c              # 清理配置
```

### 包管理
```bash
xmake require --list    # 查看依赖
xmake require --force   # 重新安装包
xmake require --clean   # 清理包缓存
```

### IDE 支持 (clangd)

项目配置了 clangd 支持，会自动生成 `compile_commands.json`：

```bash
xmake f -c && xmake
```

**注意**: `compile_commands.json` 包含机器特定路径，已添加到 `.gitignore`。每个开发者需要在本地生成。

## 项目结构

```
src/
├── cli/        # 命令行界面
├── core/       # 核心逻辑
├── discovery/  # 设备发现
├── server/     # 网络服务
└── util/       # 工具库
```

## 开发状态

- [x] 基础架构和构建系统
- [x] core核心异步io功能封装
- [x] 测试、日志与cicd
- [ ] UDP 多播设备发现
- [ ] 消息类型定义
- [ ] 基础文件传输功能
- [ ] 命令行界面
- [ ] 使用线程池提高异步性能