import { defineStore } from 'pinia'
import { secondsRemaining } from '../utils/time'

interface AuthPersist {
  playerId: number
  nickname: string
  token: string
  tokenExpireAtMs: number
  roomId: number
}

const STORAGE_KEY = 'ddz_client_auth_v1'

export const useAuthStore = defineStore('auth', {
  state: () => ({
    playerId: 0,
    nickname: '',
    coin: 0,
    roomId: 0,
    loginTicket: '',
    token: '',
    tokenExpireAtMs: 0,
    reconnectMode: false,
    loggedIn: false,
  }),
  getters: {
    tokenSecondsRemaining(state): number {
      return secondsRemaining(state.tokenExpireAtMs)
    },
    hasRecoverableSession(state): boolean {
      return state.playerId > 0 && !!state.token && state.tokenExpireAtMs > Date.now()
    },
  },
  actions: {
    loadPersist() {
      try {
        const raw = localStorage.getItem(STORAGE_KEY)
        if (!raw) {
          return
        }
        const parsed = JSON.parse(raw) as Partial<AuthPersist>
        this.playerId = parsed.playerId || 0
        this.nickname = parsed.nickname || ''
        this.token = parsed.token || ''
        this.tokenExpireAtMs = parsed.tokenExpireAtMs || 0
        this.roomId = parsed.roomId || 0
      } catch {
        localStorage.removeItem(STORAGE_KEY)
      }
    },
    savePersist() {
      const data: AuthPersist = {
        playerId: this.playerId,
        nickname: this.nickname,
        token: this.token,
        tokenExpireAtMs: this.tokenExpireAtMs,
        roomId: this.roomId,
      }
      localStorage.setItem(STORAGE_KEY, JSON.stringify(data))
    },
    clearPersist() {
      localStorage.removeItem(STORAGE_KEY)
    },
    logout() {
      this.playerId = 0
      this.nickname = ''
      this.coin = 0
      this.roomId = 0
      this.loginTicket = ''
      this.token = ''
      this.tokenExpireAtMs = 0
      this.reconnectMode = false
      this.loggedIn = false
      this.clearPersist()
    },
  },
})
