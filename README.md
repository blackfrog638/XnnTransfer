# FileSender-ByteSpark 🚀

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://en.cppreference.com/)
[![CMake](https://img.shields.io/badge/Build-CMake-brightgreen)](https://cmake.org/)

飞书文件传输模块的轻量级实现，基于C++20开发的局域网安全文件传输工具。支持加密传输、设备发现、文件完整性校验等核心功能。

## 功能特性 ✨

### 核心功能
- [x] 🛡️ **安全握手协议**：基于密码验证的客户端连接
- [x] 📡 **局域网设备发现**：自动扫描在线设备列表
- [ ] 🔒 **加密传输**：使用端到端加密
- [ ] 🔍 **文件完整性校验**：SHA-256哈希校验（支持分块校验）

## 快速开始 🚀

### 环境要求
- C++20兼容编译器（GCC 11+/Clang 14+/MSVC 2022+）
- CMake 3.20+
- [vcpkg](https://vcpkg.io) 包管理器
