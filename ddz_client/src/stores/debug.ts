import { defineStore } from 'pinia'

interface TraceItem {
  direction: 'tx' | 'rx'
  atMs: number
  text: string
}

const MAX_TRACE = 200

export const useDebugStore = defineStore('debug', {
  state: () => ({
    wsStatus: 'idle',
    message: '',
    traces: [] as TraceItem[],
  }),
  actions: {
    setStatus(status: string) {
      this.wsStatus = status
    },
    setMessage(message: string) {
      this.message = message
    },
    pushTrace(item: TraceItem) {
      this.traces.push(item)
      if (this.traces.length > MAX_TRACE) {
        this.traces.shift()
      }
    },
    clear() {
      this.traces = []
    },
  },
})
