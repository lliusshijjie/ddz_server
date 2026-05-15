export interface GatewayEnvelope {
  msg_id: number
  seq_id: number
  player_id: number
  body: string
  h5_request_id?: string
  gateway_trace_id?: string
  server_trace_id?: string
}

export interface LoginTicketResponse {
  code: number
  player_id: number
  login_ticket: string
  expire_at_ms: number
  message: string
}

export interface RoomSnapshotBase {
  room_id: number
  mode: number
  room_state: number
  players: number[]
  current_operator_player_id: number
  remain_seconds: number
  landlord_player_id: number
  base_coin: number
  snapshot_version: number
  last_action_seq: number
  players_online_bitmap: string
  trustee_players: string
  nearest_offline_deadline_ms: number
}

export interface RoomPushSnapshotV2 extends RoomSnapshotBase {
  last_play_player_id: number
  last_play_cards: number[]
  landlord_bottom_cards: number[]
  player_card_counts: Array<{ playerId: number; cardCount: number }>
  action_deadline_ms: number
}
