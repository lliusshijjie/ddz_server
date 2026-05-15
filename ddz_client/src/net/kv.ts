export function parseKv(body: string): Record<string, string> {
  const out: Record<string, string> = {}
  if (!body) {
    return out
  }
  const segments = body.split(';')
  for (const segment of segments) {
    if (!segment) {
      continue
    }
    const index = segment.indexOf('=')
    if (index <= 0) {
      continue
    }
    const key = segment.slice(0, index)
    const value = segment.slice(index + 1)
    out[key] = value
  }
  return out
}

export function buildKv(fields: Record<string, string | number | boolean | undefined | null>): string {
  const out: string[] = []
  for (const [key, value] of Object.entries(fields)) {
    if (value === undefined || value === null) {
      continue
    }
    let encoded = ''
    if (typeof value === 'boolean') {
      encoded = value ? '1' : '0'
    } else {
      encoded = String(value)
    }
    out.push(`${key}=${encoded}`)
  }
  return out.join(';')
}

export function parseI64(value: string | undefined, fallback = 0): number {
  if (value === undefined) {
    return fallback
  }
  const parsed = Number(value)
  if (!Number.isFinite(parsed)) {
    return fallback
  }
  return Math.trunc(parsed)
}

export function parseCsvNumberList(value: string | undefined): number[] {
  if (!value) {
    return []
  }
  const out: number[] = []
  const parts = value.split(',')
  for (const part of parts) {
    if (!part) {
      continue
    }
    const num = Number(part)
    if (!Number.isFinite(num)) {
      continue
    }
    out.push(Math.trunc(num))
  }
  return out
}

export function parseCardCountList(value: string | undefined): Array<{ playerId: number; cardCount: number }> {
  if (!value) {
    return []
  }
  const out: Array<{ playerId: number; cardCount: number }> = []
  const pairs = value.split(',')
  for (const pair of pairs) {
    const index = pair.indexOf(':')
    if (index <= 0) {
      continue
    }
    const playerId = Number(pair.slice(0, index))
    const cardCount = Number(pair.slice(index + 1))
    if (!Number.isFinite(playerId) || !Number.isFinite(cardCount)) {
      continue
    }
    out.push({ playerId: Math.trunc(playerId), cardCount: Math.trunc(cardCount) })
  }
  return out
}
