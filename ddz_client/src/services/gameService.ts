import { MsgId } from '../protocol/msgId'
import type { GatewayEnvelope, RoomPushSnapshotV2, RoomSnapshotBase } from '../protocol/types'
import { parseCardCountList, parseCsvNumberList, parseI64, parseKv } from '../net/kv'
import { buildKv } from '../net/kv'
import { WsClient, type WsStatus } from '../net/wsClient'

interface GameServiceConfig {
  wsBase: string
}

type PushHandler = (envelope: GatewayEnvelope) => void
type StatusHandler = (status: WsStatus) => void
type TraceHandler = (frame: {
  direction: 'tx' | 'rx'
  atMs: number
  envelope: GatewayEnvelope
}) => void

export class GameService {
  private readonly wsClient: WsClient

  public constructor(config: GameServiceConfig) {
    const wsUrl = config.wsBase.endsWith('/ws/game') ? config.wsBase : `${config.wsBase.replace(/\/$/, '')}/ws/game`
    this.wsClient = new WsClient(wsUrl)
  }

  public setHeartbeatPlayerIdProvider(provider: () => number): void {
    this.wsClient.setHeartbeatPlayerIdProvider(provider)
  }

  public onPush(handler: PushHandler): () => void {
    return this.wsClient.onPush(handler)
  }

  public onStatus(handler: StatusHandler): () => void {
    return this.wsClient.onStatus(handler)
  }

  public onTrace(handler: TraceHandler): () => void {
    return this.wsClient.onTrace(handler)
  }

  public getTraceHistory() {
    return this.wsClient.getHistory()
  }

  public async connect(): Promise<void> {
    await this.wsClient.connect()
  }

  public disconnect(): void {
    this.wsClient.disconnect()
  }

  public async login(playerId: number, loginTicket: string, nickname: string): Promise<Record<string, string>> {
    const response = await this.wsClient.sendRequest(
      MsgId.LOGIN_REQ,
      playerId,
      buildKv({
        login_ticket: loginTicket,
        nickname,
      }),
    )
    return parseKv(response.body)
  }

  public async startMatch(playerId: number, mode: number): Promise<Record<string, string>> {
    const response = await this.wsClient.sendRequest(MsgId.MATCH_REQ, playerId, buildKv({ mode }))
    return parseKv(response.body)
  }

  public async cancelMatch(playerId: number): Promise<Record<string, string>> {
    const response = await this.wsClient.sendRequest(MsgId.CANCEL_MATCH_REQ, playerId, '')
    return parseKv(response.body)
  }

  public async reconnect(
    playerId: number,
    token: string,
    roomId: number,
    snapshotVersion: number,
  ): Promise<Record<string, string>> {
    const response = await this.wsClient.sendRequest(
      MsgId.RECONNECT_REQ,
      playerId,
      buildKv({
        player_id: playerId,
        token,
        room_id: roomId,
        last_snapshot_version: snapshotVersion,
      }),
    )
    return parseKv(response.body)
  }

  public async refreshSession(playerId: number, token: string, roomId: number): Promise<Record<string, string>> {
    const response = await this.wsClient.sendRequest(
      MsgId.SESSION_REFRESH_REQ,
      playerId,
      buildKv({
        player_id: playerId,
        token,
        room_id: roomId,
      }),
    )
    return parseKv(response.body)
  }

  public async callScore(playerId: number, roomId: number, score: number): Promise<Record<string, string>> {
    const response = await this.wsClient.sendRequest(
      MsgId.PLAYER_ACTION_REQ,
      playerId,
      buildKv({
        room_id: roomId,
        action_type: 1,
        score,
      }),
    )
    return parseKv(response.body)
  }

  public async robLandlord(playerId: number, roomId: number, rob: boolean): Promise<Record<string, string>> {
    const response = await this.wsClient.sendRequest(
      MsgId.PLAYER_ACTION_REQ,
      playerId,
      buildKv({
        room_id: roomId,
        action_type: 2,
        rob: rob ? 1 : 0,
      }),
    )
    return parseKv(response.body)
  }

  public async playCards(playerId: number, roomId: number, cards: number[]): Promise<Record<string, string>> {
    const response = await this.wsClient.sendRequest(
      MsgId.PLAYER_ACTION_REQ,
      playerId,
      buildKv({
        room_id: roomId,
        action_type: 3,
        cards: cards.join(','),
      }),
    )
    return parseKv(response.body)
  }

  public async passTurn(playerId: number, roomId: number): Promise<Record<string, string>> {
    const response = await this.wsClient.sendRequest(
      MsgId.PLAYER_ACTION_REQ,
      playerId,
      buildKv({
        room_id: roomId,
        action_type: 4,
      }),
    )
    return parseKv(response.body)
  }
}

export function parseRoomSnapshotBody(body: string): RoomSnapshotBase {
  const kv = parseKv(body)
  return {
    room_id: parseI64(kv.room_id),
    mode: parseI64(kv.mode),
    room_state: parseI64(kv.room_state),
    players: parseCsvNumberList(kv.players),
    current_operator_player_id: parseI64(kv.current_operator_player_id),
    remain_seconds: parseI64(kv.remain_seconds),
    landlord_player_id: parseI64(kv.landlord_player_id),
    base_coin: parseI64(kv.base_coin),
    snapshot_version: parseI64(kv.snapshot_version),
    last_action_seq: parseI64(kv.last_action_seq),
    players_online_bitmap: kv.players_online_bitmap || '',
    trustee_players: kv.trustee_players || '',
    nearest_offline_deadline_ms: parseI64(kv.nearest_offline_deadline_ms),
  }
}

export function parseRoomPushV2Body(body: string): RoomPushSnapshotV2 {
  const base = parseRoomSnapshotBody(body)
  const kv = parseKv(body)
  return {
    ...base,
    last_play_player_id: parseI64(kv.last_play_player_id),
    last_play_cards: parseCsvNumberList(kv.last_play_cards),
    landlord_bottom_cards: parseCsvNumberList(kv.landlord_bottom_cards),
    player_card_counts: parseCardCountList(kv.player_card_counts),
    action_deadline_ms: parseI64(kv.action_deadline_ms),
  }
}

export function parsePushBody(body: string): Record<string, string> {
  return parseKv(body)
}
