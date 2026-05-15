import { MsgId } from '../protocol/msgId'
import type { GatewayEnvelope } from '../protocol/types'
import { toEnvelopeText, parseEnvelopeText } from './envelope'
import { buildKv } from './kv'
import { generateGatewayTraceId, generateH5RequestId } from '../utils/requestId'

export type WsStatus = 'idle' | 'connecting' | 'connected' | 'reconnecting' | 'disconnected'

interface PendingRequest {
  resolve: (value: GatewayEnvelope) => void
  reject: (reason?: unknown) => void
  timer: ReturnType<typeof setTimeout>
}

interface WsClientOptions {
  heartbeatIntervalMs: number
  requestTimeoutMs: number
  maxHistory: number
}

interface TraceFrame {
  direction: 'tx' | 'rx'
  atMs: number
  envelope: GatewayEnvelope
}

const DEFAULT_OPTIONS: WsClientOptions = {
  heartbeatIntervalMs: 10000,
  requestTimeoutMs: 8000,
  maxHistory: 120,
}

export class WsClient {
  private readonly url: string
  private readonly options: WsClientOptions
  private ws: WebSocket | null = null
  private status: WsStatus = 'idle'
  private pending = new Map<number, PendingRequest>()
  private nextSeq = 1
  private reconnectAttempt = 0
  private manuallyClosed = false
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null
  private heartbeatTimer: ReturnType<typeof setInterval> | null = null
  private statusHandlers = new Set<(status: WsStatus) => void>()
  private pushHandlers = new Set<(envelope: GatewayEnvelope) => void>()
  private traceHandlers = new Set<(frame: TraceFrame) => void>()
  private history: TraceFrame[] = []
  private heartbeatPlayerIdProvider: () => number = () => 0

  public constructor(url: string, options?: Partial<WsClientOptions>) {
    this.url = url
    this.options = { ...DEFAULT_OPTIONS, ...(options || {}) }
  }

  public setHeartbeatPlayerIdProvider(provider: () => number): void {
    this.heartbeatPlayerIdProvider = provider
  }

  public onStatus(handler: (status: WsStatus) => void): () => void {
    this.statusHandlers.add(handler)
    handler(this.status)
    return () => {
      this.statusHandlers.delete(handler)
    }
  }

  public onPush(handler: (envelope: GatewayEnvelope) => void): () => void {
    this.pushHandlers.add(handler)
    return () => {
      this.pushHandlers.delete(handler)
    }
  }

  public onTrace(handler: (frame: TraceFrame) => void): () => void {
    this.traceHandlers.add(handler)
    return () => {
      this.traceHandlers.delete(handler)
    }
  }

  public getHistory(): TraceFrame[] {
    return [...this.history]
  }

  public currentStatus(): WsStatus {
    return this.status
  }

  public async connect(): Promise<void> {
    if (this.status === 'connected') {
      return
    }
    if (this.status === 'connecting' || this.status === 'reconnecting') {
      return
    }
    this.manuallyClosed = false
    this.setStatus(this.reconnectAttempt > 0 ? 'reconnecting' : 'connecting')
    await this.openSocket()
  }

  public disconnect(): void {
    this.manuallyClosed = true
    this.clearReconnectTimer()
    this.clearHeartbeatTimer()
    if (this.ws) {
      this.ws.close()
      this.ws = null
    }
    this.rejectAllPending(new Error('websocket disconnected'))
    this.setStatus('disconnected')
  }

  public async sendRequest(
    msgId: number,
    playerId: number,
    body: string,
    timeoutMs?: number,
    gatewayTraceId = '',
  ): Promise<GatewayEnvelope> {
    if (!this.ws || this.status !== 'connected') {
      throw new Error('websocket not connected')
    }
    const seqId = this.nextSeq++
    const envelope: GatewayEnvelope = {
      msg_id: msgId,
      seq_id: seqId,
      player_id: playerId,
      body,
      h5_request_id: generateH5RequestId(),
      gateway_trace_id: gatewayTraceId || generateGatewayTraceId(),
    }
    const text = toEnvelopeText(envelope)
    this.ws.send(text)
    this.recordTrace('tx', envelope)
    return new Promise<GatewayEnvelope>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(seqId)
        reject(new Error(`request timeout msg_id=${msgId} seq_id=${seqId}`))
      }, timeoutMs || this.options.requestTimeoutMs)
      this.pending.set(seqId, { resolve, reject, timer })
    })
  }

  public sendFireAndForget(msgId: number, playerId: number, body: string): void {
    if (!this.ws || this.status !== 'connected') {
      return
    }
    const envelope: GatewayEnvelope = {
      msg_id: msgId,
      seq_id: this.nextSeq++,
      player_id: playerId,
      body,
      h5_request_id: generateH5RequestId(),
      gateway_trace_id: generateGatewayTraceId(),
    }
    this.ws.send(toEnvelopeText(envelope))
    this.recordTrace('tx', envelope)
  }

  private setStatus(status: WsStatus): void {
    this.status = status
    for (const handler of this.statusHandlers) {
      handler(status)
    }
  }

  private async openSocket(): Promise<void> {
    await new Promise<void>((resolve, reject) => {
      const ws = new WebSocket(this.url)
      this.ws = ws
      ws.onopen = () => {
        this.reconnectAttempt = 0
        this.setStatus('connected')
        this.startHeartbeat()
        resolve()
      }
      ws.onerror = () => {
        reject(new Error('websocket connect failed'))
      }
      ws.onmessage = (event) => {
        this.handleMessage(String(event.data))
      }
      ws.onclose = () => {
        this.clearHeartbeatTimer()
        this.ws = null
        this.rejectAllPending(new Error('websocket closed'))
        if (this.manuallyClosed) {
          this.setStatus('disconnected')
          return
        }
        this.setStatus('reconnecting')
        this.scheduleReconnect()
      }
    })
  }

  private scheduleReconnect(): void {
    this.clearReconnectTimer()
    this.reconnectAttempt += 1
    const backoff = Math.min(8000, 500 * Math.pow(2, this.reconnectAttempt))
    this.reconnectTimer = setTimeout(() => {
      this.connect().catch(() => {
        this.scheduleReconnect()
      })
    }, backoff)
  }

  private clearReconnectTimer(): void {
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer)
      this.reconnectTimer = null
    }
  }

  private startHeartbeat(): void {
    this.clearHeartbeatTimer()
    this.heartbeatTimer = setInterval(() => {
      if (this.status !== 'connected') {
        return
      }
      this.sendFireAndForget(
        MsgId.HEARTBEAT_REQ,
        this.heartbeatPlayerIdProvider(),
        buildKv({ ts: Date.now() }),
      )
    }, this.options.heartbeatIntervalMs)
  }

  private clearHeartbeatTimer(): void {
    if (this.heartbeatTimer) {
      clearInterval(this.heartbeatTimer)
      this.heartbeatTimer = null
    }
  }

  private handleMessage(text: string): void {
    let envelope: GatewayEnvelope
    try {
      envelope = parseEnvelopeText(text)
    } catch {
      return
    }
    this.recordTrace('rx', envelope)
    if (envelope.seq_id > 0) {
      const pending = this.pending.get(envelope.seq_id)
      if (pending) {
        clearTimeout(pending.timer)
        this.pending.delete(envelope.seq_id)
        pending.resolve(envelope)
        return
      }
    }
    for (const handler of this.pushHandlers) {
      handler(envelope)
    }
  }

  private rejectAllPending(reason: Error): void {
    for (const [seqId, pending] of this.pending.entries()) {
      clearTimeout(pending.timer)
      pending.reject(reason)
      this.pending.delete(seqId)
    }
  }

  private recordTrace(direction: 'tx' | 'rx', envelope: GatewayEnvelope): void {
    const frame: TraceFrame = {
      direction,
      atMs: Date.now(),
      envelope,
    }
    this.history.push(frame)
    if (this.history.length > this.options.maxHistory) {
      this.history.shift()
    }
    for (const handler of this.traceHandlers) {
      handler(frame)
    }
  }
}
