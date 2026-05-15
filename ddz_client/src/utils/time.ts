export function nowMs(): number {
  return Date.now()
}

export function secondsRemaining(targetMs: number): number {
  if (targetMs <= 0) {
    return 0
  }
  const diff = targetMs - nowMs()
  return Math.max(0, Math.floor(diff / 1000))
}
