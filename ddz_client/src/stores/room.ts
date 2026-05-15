import { defineStore } from 'pinia'
import type { RoomPushSnapshotV2, RoomSnapshotBase } from '../protocol/types'

interface PrivateHand {
  cards: number[]
  cardCount: number
  snapshotVersion: number
}

export const useRoomStore = defineStore('room', {
  state: () => ({
    inRoom: false,
    reconnectNoticePlayerId: 0,
    base: {
      room_id: 0,
      mode: 0,
      room_state: 0,
      players: [],
      current_operator_player_id: 0,
      remain_seconds: 0,
      landlord_player_id: 0,
      base_coin: 0,
      snapshot_version: 0,
      last_action_seq: 0,
      players_online_bitmap: '',
      trustee_players: '',
      nearest_offline_deadline_ms: 0,
    } as RoomSnapshotBase,
    v2: null as RoomPushSnapshotV2 | null,
    privateHand: {
      cards: [],
      cardCount: 0,
      snapshotVersion: 0,
    } as PrivateHand,
  }),
  getters: {
    roomId(state): number {
      return state.base.room_id
    },
  },
  actions: {
    reset() {
      this.inRoom = false
      this.reconnectNoticePlayerId = 0
      this.base = {
        room_id: 0,
        mode: 0,
        room_state: 0,
        players: [],
        current_operator_player_id: 0,
        remain_seconds: 0,
        landlord_player_id: 0,
        base_coin: 0,
        snapshot_version: 0,
        last_action_seq: 0,
        players_online_bitmap: '',
        trustee_players: '',
        nearest_offline_deadline_ms: 0,
      }
      this.v2 = null
      this.privateHand = {
        cards: [],
        cardCount: 0,
        snapshotVersion: 0,
      }
    },
    setBase(snapshot: RoomSnapshotBase) {
      this.base = snapshot
      this.inRoom = snapshot.room_id > 0
    },
    setV2(snapshot: RoomPushSnapshotV2) {
      this.v2 = snapshot
      this.base = {
        room_id: snapshot.room_id,
        mode: snapshot.mode,
        room_state: snapshot.room_state,
        players: snapshot.players,
        current_operator_player_id: snapshot.current_operator_player_id,
        remain_seconds: snapshot.remain_seconds,
        landlord_player_id: snapshot.landlord_player_id,
        base_coin: snapshot.base_coin,
        snapshot_version: snapshot.snapshot_version,
        last_action_seq: snapshot.last_action_seq,
        players_online_bitmap: snapshot.players_online_bitmap,
        trustee_players: snapshot.trustee_players,
        nearest_offline_deadline_ms: snapshot.nearest_offline_deadline_ms,
      }
      this.inRoom = snapshot.room_id > 0
    },
    setPrivateHand(cards: number[], cardCount: number, snapshotVersion: number) {
      this.privateHand = {
        cards,
        cardCount,
        snapshotVersion,
      }
    },
  },
})
