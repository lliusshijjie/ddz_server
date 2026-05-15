import { defineStore } from 'pinia'

export const useLobbyStore = defineStore('lobby', {
  state: () => ({
    matching: false,
    matched: false,
    timeoutNotified: false,
    mode: 3,
    matchedRoomId: 0,
    matchedPlayers: [] as number[],
  }),
  actions: {
    reset() {
      this.matching = false
      this.matched = false
      this.timeoutNotified = false
      this.mode = 3
      this.matchedRoomId = 0
      this.matchedPlayers = []
    },
  },
})
