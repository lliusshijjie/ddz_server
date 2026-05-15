# ddz_client

基于 Vue3 + Vite + TypeScript 的斗地主 H5 MVP 客户端，协议适配 `ddz_server` 网关。

## 环境变量

开发环境读取 `.env.development`：

- `VITE_GATEWAY_HTTP`：网关 HTTP 地址（登录换票）
- `VITE_GATEWAY_WS`：网关 WS 基地址（客户端自动拼接 `/ws/game`）
- `VITE_DEV_KEY`：登录换票接口 Header `X-Dev-Key`

## 启动

```powershell
npm install
npm run dev
```

## 构建

```powershell
npm run build
```

## MVP 覆盖能力

- 登录与换票（HTTP）
- 匹配与取消匹配
- 房间快照与手牌同步
- 叫分/抢地主/出牌/过牌
- 断线重连与会话刷新
- 结算展示
- 调试面板（WS 状态 + 收发包追踪）
# Vue 3 + TypeScript + Vite

This template should help get you started developing with Vue 3 and TypeScript in Vite. The template uses Vue 3 `<script setup>` SFCs, check out the [script setup docs](https://v3.vuejs.org/api/sfc-script-setup.html#sfc-script-setup) to learn more.

Learn more about the recommended Project Setup and IDE Support in the [Vue Docs TypeScript Guide](https://vuejs.org/guide/typescript/overview.html#project-setup).
