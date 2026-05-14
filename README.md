# ddz_server

斗地主 C++17 服务端工程，采用服务端权威判定与可审计持久化路线。

## 已实现能力

- P0-P3：TCP/协议编解码、消息分发、登录会话、匹配入房、断线重连（强校验 + 快照版本冲突恢复）
- P4：规则引擎（牌型识别/比较）、动作语义校验、服务端自动触发结算
- P6：MySQL DAO 与结算事务、Redis token/session/online 接入
- P5（本次）：可观测埋点、压测脚本、CI 门禁、故障演练测试

## 环境要求

- CMake >= 3.16
- C++17 编译器（Windows 可用 MSVC）
- Docker Desktop（本地 MySQL/Redis）

## 本地运行

```bash
docker compose up -d
cmake -S . -B build
cmake --build build --config Debug
ctest -C Debug --test-dir build --output-on-failure
build/Debug/ddz_server.exe config/dev/server.yaml
```

## P5 工程化命令

本地门禁：

```powershell
tools/ci/check.ps1 -BuildDir build -Config Debug
```

压测软门槛：

```powershell
tools/perf/run_stress.ps1 -BuildDir build -Config Debug -SoftThresholdEnabled $true
```

故障演练：

```bash
ctest -C Debug --test-dir build -R p5_fault_drills --output-on-failure
```

## 协议说明

包结构：

```text
| packet_len(4) | msg_id(4) | seq_id(4) | player_id(8) | body(N) |
```

`body` 为 KV 文本：`k1=v1;k2=v2`。

关键 P4/P5 消息：

- `4001` `MSG_PLAYER_ACTION_REQ`
- `4002` `MSG_PLAYER_ACTION_RESP`
- `4003` `MSG_ROOM_STATE_PUSH`
- `4004` `MSG_GAME_RESULT_PUSH`
- `5001` `MSG_GAME_OVER_NOTIFY`

说明：`5002 MSG_SETTLEMENT_REQ` 已禁止客户端权威结算，服务端只接受内部自动结算链路。

关键 P3 消息：

- `3001` `MSG_ROOM_SNAPSHOT_NOTIFY`
- `3003` `MSG_PLAYER_RECONNECT_NOTIFY`
- `6001` `MSG_RECONNECT_REQ`
- `6002` `MSG_RECONNECT_RESP`

`MSG_RECONNECT_REQ` 当前字段：

- `player_id`
- `token`
- `room_id`
- `last_snapshot_version`

当客户端版本落后或冲突时，服务端返回 `SNAPSHOT_VERSION_CONFLICT` 并附最新快照，客户端可直接覆盖本地态。

## P3 离线策略

- 玩家离线 `>=30s`：进入托管（最小可行策略自动动作）
- 玩家离线 `>=120s`：按超时判负触发服务端结算
- 快照包含 `snapshot_version/last_action_seq/players_online_bitmap/trustee_players`

## 配置新增（P5）

```yaml
observability:
  enable_structured_log: true
  metrics_report_interval_ms: 30000

perf:
  soft_threshold_enabled: true
```

CI：`.github/workflows/ci.yml`（Ubuntu + Windows，build + ctest + perf soft gate）。
