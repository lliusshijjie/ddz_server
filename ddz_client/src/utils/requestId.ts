export function generateH5RequestId(): string {
  const random = Math.floor(Math.random() * 1000000)
  return `c-${Date.now()}-${random}`
}

export function generateGatewayTraceId(): string {
  const random = Math.floor(Math.random() * 1000000)
  return `gw-${Date.now()}-${random}`
}
