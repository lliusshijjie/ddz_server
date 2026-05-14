# ddz_server

一个用于斗地主最小可行服务端的 C++17 项目，包含：

- 基础 TCP 网络层（连接管理、心跳、消息编解码）
- 登录、匹配、重连、结算等核心服务
- 基于 CMake + CTest 的分层测试

## 环境要求

- CMake >= 3.16
- 支持 C++17 的编译器（Windows 下可用 MSVC）

## 构建

```bash
cmake -S . -B build
cmake --build build --config Debug
```

## 测试

```bash
ctest -C Debug --test-dir build --output-on-failure
```

## 运行

```bash
build/Debug/ddz_server.exe config/dev/server.yaml
```

## 最近修复

- 修复连接关闭时读线程生命周期问题，避免对象过早析构。
- 修复服务端停止时 `accept()` 可能阻塞导致无法优雅退出的问题。
- 增加结算请求发起者房间归属校验，防止跨房间伪造结算。
- 增加同房间并发结算串行保护，避免重复结算。
- 修复登录流程重复登录时金币被重置为初始值的问题。

