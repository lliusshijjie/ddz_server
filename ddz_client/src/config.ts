export const appConfig = {
  gatewayHttp: import.meta.env.VITE_GATEWAY_HTTP || 'http://127.0.0.1:9010',
  gatewayWs: import.meta.env.VITE_GATEWAY_WS || 'ws://127.0.0.1:9010',
  devKey: import.meta.env.VITE_DEV_KEY || 'dev_gateway_key',
}
