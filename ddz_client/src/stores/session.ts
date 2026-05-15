import { defineStore } from 'pinia'
import { appConfig } from '../config'
import { parseCsvNumberList, parseI64, parseKv } from '../net/kv'
import { ErrorCode, toErrorCode } from '../protocol/errorCode'
import { MsgId } from '../protocol/msgId'
import { AuthService } from '../services/authService'
import { GameService, parsePushBody, parseRoomPushV2Body, parseRoomSnapshotBody } from '../services/gameService'
import { useAuthStore } from './auth'
import { useDebugStore } from './debug'
import { useLobbyStore } from './lobby'
import { useRoomStore } from './room'
import { useSettlementStore } from './settlement'

const authService = new AuthService(appConfig.gatewayHttp, appConfig.devKey)
const gameService = new GameService({ wsBase: appConfig.gatewayWs })

export const useSessionStore = defineStore('session', {
  state: () => ({
    initialized: false,
    busy: false,
    error: '',
    recoveringAfterReconnect: false,
    refreshTimer: null as ReturnType<typeof setInterval> | null,
  }),
  actions: {
    init() {
      if (this.initialized) {
        return
      }
      const auth = useAuthStore()
      const debug = useDebugStore()
      auth.loadPersist()
      gameService.setHeartbeatPlayerIdProvider(() => auth.playerId)
      gameService.onStatus((status) => {
        debug.setStatus(status)
        if (status !== 'connected') {
          return
        }
        if (!auth.loggedIn || auth.roomId <= 0 || this.recoveringAfterReconnect) {
          return
        }
        this.recoveringAfterReconnect = true
        this.reconnect(auth.roomId, useRoomStore().base.snapshot_version)
          .catch(() => {})
          .finally(() => {
            this.recoveringAfterReconnect = false
          })
      })
      gameService.onTrace((frame) => {
        debug.pushTrace({
          direction: frame.direction,
          atMs: frame.atMs,
          text: JSON.stringify(frame.envelope),
        })
      })
      gameService.onPush((envelope) => {
        this.handlePush(envelope.msg_id, envelope.body)
      })
      this.initialized = true
    },
    async ensureConnected() {
      try {
        await gameService.connect()
      } catch (error) {
        const message = error instanceof Error ? error.message : 'gateway connect failed'
        this.error = message
        throw error
      }
    },
    async login(playerId: number, nickname: string) {
      const auth = useAuthStore()
      const lobby = useLobbyStore()
      const room = useRoomStore()
      const settlement = useSettlementStore()
      const debug = useDebugStore()
      this.busy = true
      this.error = ''
      this.recoveringAfterReconnect = false
      try {
        const ticketResp = await authService.fetchLoginTicket(playerId, nickname)
        if (ticketResp.code !== 0 || !ticketResp.login_ticket) {
          throw new Error(ticketResp.message || 'fetch login ticket failed')
        }
        auth.loginTicket = ticketResp.login_ticket
        await this.ensureConnected()
        const loginKv = await gameService.login(playerId, ticketResp.login_ticket, nickname)
        const loginCode = toErrorCode(loginKv.code)
        if (loginCode !== ErrorCode.OK) {
          throw new Error(loginKv.reason || `login failed code=${loginKv.code || -1}`)
        }
        auth.playerId = parseI64(loginKv.player_id, playerId)
        auth.nickname = loginKv.nickname || nickname || `player_${playerId}`
        auth.coin = parseI64(loginKv.coin, 0)
        auth.roomId = parseI64(loginKv.room_id, 0)
        auth.reconnectMode = loginKv.reconnect_mode === '1'
        auth.token = loginKv.token || ''
        auth.tokenExpireAtMs = parseI64(loginKv.expire_at_ms, 0)
        auth.loggedIn = true
        auth.savePersist()
        lobby.reset()
        room.reset()
        settlement.clear()
        debug.setMessage(`登录成功 player_id=${auth.playerId}`)
        this.startRefreshTimer()
        if (auth.reconnectMode && auth.roomId > 0) {
          await this.reconnect(auth.roomId, room.base.snapshot_version)
        }
      } finally {
        this.busy = false
      }
    },
    async startMatch() {
      const auth = useAuthStore()
      const lobby = useLobbyStore()
      this.busy = true
      this.error = ''
      try {
        const kv = await gameService.startMatch(auth.playerId, lobby.mode)
        const code = toErrorCode(kv.code)
        if (code !== ErrorCode.OK) {
          throw new Error(kv.reason || `match failed code=${kv.code || -1}`)
        }
        lobby.matching = true
        lobby.timeoutNotified = false
      } finally {
        this.busy = false
      }
    },
    async cancelMatch() {
      const auth = useAuthStore()
      const lobby = useLobbyStore()
      this.busy = true
      this.error = ''
      try {
        const kv = await gameService.cancelMatch(auth.playerId)
        const code = toErrorCode(kv.code)
        if (code !== ErrorCode.OK) {
          throw new Error(kv.reason || `cancel match failed code=${kv.code || -1}`)
        }
        lobby.matching = false
      } finally {
        this.busy = false
      }
    },
    async reconnect(roomId: number, snapshotVersion: number) {
      const auth = useAuthStore()
      this.busy = true
      this.error = ''
      try {
        const kv = await gameService.reconnect(auth.playerId, auth.token, roomId, snapshotVersion)
        const code = toErrorCode(kv.code)
        if (code !== ErrorCode.OK && code !== ErrorCode.SNAPSHOT_VERSION_CONFLICT) {
          throw new Error(kv.reason || `reconnect failed code=${kv.code || -1}`)
        }
      } finally {
        this.busy = false
      }
    },
    async refreshSession() {
      const auth = useAuthStore()
      if (!auth.loggedIn || !auth.token || auth.roomId <= 0) {
        return
      }
      const kv = await gameService.refreshSession(auth.playerId, auth.token, auth.roomId)
      const code = toErrorCode(kv.code)
      if (code !== ErrorCode.OK) {
        return
      }
      auth.token = kv.token || auth.token
      auth.tokenExpireAtMs = parseI64(kv.expire_at_ms, auth.tokenExpireAtMs)
      auth.roomId = parseI64(kv.room_id, auth.roomId)
      auth.savePersist()
    },
    async callScore(score: number) {
      const auth = useAuthStore()
      const room = useRoomStore()
      this.error = ''
      const kv = await gameService.callScore(auth.playerId, room.base.room_id, score)
      const code = toErrorCode(kv.code)
      if (code !== ErrorCode.OK) {
        throw new Error(kv.reason || 'call score failed')
      }
    },
    async robLandlord(rob: boolean) {
      const auth = useAuthStore()
      const room = useRoomStore()
      this.error = ''
      const kv = await gameService.robLandlord(auth.playerId, room.base.room_id, rob)
      const code = toErrorCode(kv.code)
      if (code !== ErrorCode.OK) {
        throw new Error(kv.reason || 'rob landlord failed')
      }
    },
    async playCards(cards: number[]) {
      const auth = useAuthStore()
      const room = useRoomStore()
      this.error = ''
      const kv = await gameService.playCards(auth.playerId, room.base.room_id, cards)
      const code = toErrorCode(kv.code)
      if (code !== ErrorCode.OK) {
        throw new Error(kv.reason || 'play cards failed')
      }
    },
    async passTurn() {
      const auth = useAuthStore()
      const room = useRoomStore()
      this.error = ''
      const kv = await gameService.passTurn(auth.playerId, room.base.room_id)
      const code = toErrorCode(kv.code)
      if (code !== ErrorCode.OK) {
        throw new Error(kv.reason || 'pass failed')
      }
    },
    logout() {
      const auth = useAuthStore()
      const lobby = useLobbyStore()
      const room = useRoomStore()
      const settlement = useSettlementStore()
      gameService.disconnect()
      auth.logout()
      lobby.reset()
      room.reset()
      settlement.clear()
      this.error = ''
      this.recoveringAfterReconnect = false
      this.stopRefreshTimer()
    },
    handlePush(msgId: number, body: string) {
      const auth = useAuthStore()
      const lobby = useLobbyStore()
      const room = useRoomStore()
      const settlement = useSettlementStore()
      const kv = parsePushBody(body)
      switch (msgId) {
        case MsgId.MATCH_SUCCESS_NOTIFY:
          lobby.matching = false
          lobby.matched = true
          lobby.matchedRoomId = parseI64(kv.room_id, 0)
          lobby.matchedPlayers = parseCsvNumberList(kv.players)
          auth.roomId = lobby.matchedRoomId
          auth.savePersist()
          break
        case MsgId.MATCH_TIMEOUT_NOTIFY:
          lobby.matching = false
          lobby.timeoutNotified = true
          break
        case MsgId.ROOM_SNAPSHOT_NOTIFY:
        case MsgId.ROOM_STATE_PUSH:
          room.setBase(parseRoomSnapshotBody(body))
          auth.roomId = room.base.room_id
          auth.savePersist()
          break
        case MsgId.ROOM_STATE_PUSH_V2:
          room.setV2(parseRoomPushV2Body(body))
          auth.roomId = room.base.room_id
          auth.savePersist()
          break
        case MsgId.PRIVATE_HAND_NOTIFY:
          room.setPrivateHand(
            parseCsvNumberList(kv.cards),
            parseI64(kv.card_count, 0),
            parseI64(kv.snapshot_version, 0),
          )
          break
        case MsgId.PLAYER_RECONNECT_NOTIFY:
          room.reconnectNoticePlayerId = parseI64(kv.player_id, 0)
          break
        case MsgId.GAME_OVER_NOTIFY:
        case MsgId.GAME_RESULT_PUSH:
          settlement.setLatest({
            roomId: parseI64(kv.room_id, 0),
            winnerPlayerId: parseI64(kv.winner_player_id, 0),
            coinChange: parseI64(kv.coin_change, 0),
            afterCoin: parseI64(kv.after_coin, 0),
          })
          auth.coin = parseI64(kv.after_coin, auth.coin)
          auth.roomId = 0
          auth.savePersist()
          break
        case MsgId.HEARTBEAT_RESP:
        case MsgId.LOGIN_RESP:
        case MsgId.MATCH_RESP:
        case MsgId.CANCEL_MATCH_RESP:
        case MsgId.RECONNECT_RESP:
        case MsgId.SESSION_REFRESH_RESP:
        case MsgId.PLAYER_ACTION_RESP:
        default:
          break
      }
    },
    startRefreshTimer() {
      this.stopRefreshTimer()
      this.refreshTimer = setInterval(() => {
        const auth = useAuthStore()
        if (!auth.loggedIn) {
          return
        }
        if (auth.tokenSecondsRemaining <= 120) {
          this.refreshSession().catch(() => {})
        }
      }, 10000)
    },
    stopRefreshTimer() {
      if (this.refreshTimer) {
        clearInterval(this.refreshTimer)
        this.refreshTimer = null
      }
    },
    consumeResponseBody(body: string): Record<string, string> {
      return parseKv(body)
    },
  },
})
