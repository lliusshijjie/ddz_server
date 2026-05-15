import { defineStore } from 'pinia'

export interface SettlementInfo {
  roomId: number
  winnerPlayerId: number
  coinChange: number
  afterCoin: number
}

export const useSettlementStore = defineStore('settlement', {
  state: () => ({
    latest: null as SettlementInfo | null,
  }),
  actions: {
    clear() {
      this.latest = null
    },
    setLatest(info: SettlementInfo) {
      this.latest = info
    },
  },
})
