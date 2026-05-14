# ddz_server

斗地主 C++17 服务端，已完成 P0~P6 主链路，核心目标是服务端权威裁决与可审计持久化。

## 已实现能力

- P0~P3：TCP/协议编解码/消息分发、登录会话、匹配入房、断线重连快照。
- P4：动作状态机（叫分/抢地主/出牌/过牌）、规则引擎、服务端内部触发结算。
- P6：MySQL 持久化（DAO SQL 化）、结算事务落库、Redis token/session/online 接入。

## 环境要求

- CMake >= 3.16
- 支持 C++17 的编译器（Windows 可用 MSVC）
- Docker Desktop（用于本地 MySQL/Redis）

## 本地运行

```bash
docker compose up -d
cmake -S . -B build
cmake --build build --config Debug
ctest -C Debug --test-dir build --output-on-failure
build/Debug/ddz_server.exe config/dev/server.yaml
```

## 协议说明

包结构：

```text
| packet_len(4) | msg_id(4) | seq_id(4) | player_id(8) | body(N) |
```

`body` 使用 KV 文本格式：`k1=v1;k2=v2`。

### 关键消息 ID

- 登录/心跳：`1001/1002/1101/1102`
- 匹配：`2001/2002/2003/2004/2005/2006`
- 重连/快照：`3001/3003/6001/6002`
- P4 动作链路：`4001`(`MSG_PLAYER_ACTION_REQ`)、`4002`(`RESP`)、`4003`(`MSG_ROOM_STATE_PUSH`)、`4004`(`MSG_GAME_RESULT_PUSH`)
- 结算广播：`5001`(`MSG_GAME_OVER_NOTIFY`)

说明：`5002 MSG_SETTLEMENT_REQ` 不再作为客户端权威入口，服务端会拒绝客户端直接结算请求。

## 配置要点

`config/dev/server.yaml` 关键字段：

```yaml
mysql:
  enabled: true
  host: 127.0.0.1
  port: 3306
  user: ddz
  password: ddz_pass
  database: ddz
  pool_size: 4

redis:
  enabled: false
  host: 127.0.0.1
  port: 6379
  db: 0

auth:
  token_secret: "dev_change_me"
  token_ttl_seconds: 604800
```

开启 `redis.enabled=true` 后，登录/重连链路按 fail-closed 策略依赖 Redis token/session 一致性校验。
