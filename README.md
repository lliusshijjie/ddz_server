# ddz_server

一个用于斗地主服务端演进的 C++17 项目，当前重点是服务端基础能力与权威状态管理。

已实现的主链路（当前阶段）：

- TCP 长连接、心跳、协议编解码、消息分发
- 登录与会话管理（P1）
- 匹配与房间管理（P2）
- 断线重连基础能力（P3）
- 结算与存储骨架（后续继续强化）

## 环境要求

- CMake >= 3.16
- 支持 C++17 的编译器（Windows 可用 MSVC）

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

## 协议说明（当前）

包结构：

```text
| packet_len(4) | msg_id(4) | seq_id(4) | player_id(8) | body(N) |
```

`body` 使用 KV 文本格式：`k1=v1;k2=v2`。

### 消息 ID

- `1001` `MSG_LOGIN_REQ`
- `1002` `MSG_LOGIN_RESP`
- `1101` `MSG_HEARTBEAT_REQ`
- `1102` `MSG_HEARTBEAT_RESP`
- `2001` `MSG_MATCH_REQ`
- `2002` `MSG_MATCH_RESP`
- `2003` `MSG_CANCEL_MATCH_REQ`
- `2004` `MSG_CANCEL_MATCH_RESP`
- `2005` `MSG_MATCH_SUCCESS_NOTIFY`
- `2006` `MSG_MATCH_TIMEOUT_NOTIFY`
- `3001` `MSG_ROOM_SNAPSHOT_NOTIFY`
- `3003` `MSG_PLAYER_RECONNECT_NOTIFY`
- `5001` `MSG_GAME_OVER_NOTIFY`
- `5002` `MSG_SETTLEMENT_REQ`
- `5003` `MSG_SETTLEMENT_RESP`
- `6001` `MSG_RECONNECT_REQ`
- `6002` `MSG_RECONNECT_RESP`

## P1：登录与会话（已实现）

### 登录请求/响应

- 请求（`1001`）：
  - `body`: `player_id=<id>;nickname=<name>`（`nickname` 可选）
- 响应（`1002`）：
  - `code`
  - `player_id`
  - `nickname`
  - `coin`
  - `token`
  - `expire_at_ms`

### Token 规则

- 格式：`v1.player_id.expire_at_ms.nonce.sig`
- 签名：服务端基于 `auth.token_secret` 生成
- 重连必须使用该 token 校验签名与过期时间
- 不再兼容旧格式 `token=<player_id>`

### 重复登录策略

- 同账号新登录会顶掉旧连接（踢旧连保新连）

## P2：匹配与房间（已实现）

### 匹配请求/响应

- 请求（`2001`）：`body: mode=3` 或 `mode=4`
- 响应（`2002`）：`code`

### 匹配成功通知

- 通知（`2005`）：`room_id`、`mode`、`players`

### 匹配超时

- 服务端按 `server.match_timeout_ms` 自动超时出队
- 玩家状态自动回到 Lobby
- 通知（`2006`）：`code=MATCH_TIMEOUT`、`mode`

## 配置项

`config/dev/server.yaml` 关键字段：

```yaml
server:
  host: 0.0.0.0
  port: 9000
  io_threads: 2
  max_packet_size: 65536
  heartbeat_timeout_ms: 30000
  match_timeout_ms: 30000

log:
  level: info
  dir: ./logs

auth:
  token_secret: "dev_change_me"
  token_ttl_seconds: 604800
```

## 当前注意事项

- 玩法权威裁决（叫分/抢地主/出牌规则引擎）尚未完整落地。
- 持久化与事务目前是演进中能力，后续继续推进到生产级一致性。
