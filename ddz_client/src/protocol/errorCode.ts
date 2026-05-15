export const ErrorCode = {
  OK: 0,
  UNKNOWN_ERROR: 1,
  INVALID_PACKET: 2,
  INVALID_TOKEN: 3,
  NOT_LOGIN: 4,
  ALREADY_LOGIN: 5,
  TOKEN_EXPIRED: 6,
  INVALID_PLAYER_STATE: 1002,
  PLAYER_ALREADY_MATCHING: 1003,
  PLAYER_ALREADY_IN_ROOM: 1004,
  MATCH_MODE_INVALID: 2001,
  MATCH_CANCEL_FAILED: 2002,
  MATCH_TIMEOUT: 2003,
  ROOM_NOT_FOUND: 3001,
  SETTLEMENT_FAILED: 5002,
  RECONNECT_ROOM_MISMATCH: 6003,
  SNAPSHOT_VERSION_CONFLICT: 6004,
  OFFLINE_TIMEOUT_LOSE: 6005,
} as const

export type ErrorCode = (typeof ErrorCode)[keyof typeof ErrorCode]

export function toErrorCode(value: string | number | undefined): ErrorCode {
  const numeric = typeof value === 'string' ? Number(value) : value
  if (typeof numeric !== 'number' || Number.isNaN(numeric)) {
    return ErrorCode.UNKNOWN_ERROR
  }
  return numeric as ErrorCode
}
