# DDZ

斗地主服务端 + H5 客户端同仓维护。

| 目录 | 说明 |
|------|------|
| [`ddz_server/`](ddz_server/) | C++ 网关与房间服务端（详见该目录 README） |
| [`ddz_client/`](ddz_client/) | Vue + Vite H5 客户端 |

## 客户端本地运行

```powershell
cd ddz_client
npm install
npm run dev
```

默认开发地址：<http://localhost:5173/>

## 服务端

构建、配置与协议说明见 [`ddz_server/README.md`](ddz_server/README.md)。
