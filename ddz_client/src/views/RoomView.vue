<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { useRouter } from 'vue-router'
import ActionBar from '../components/ActionBar.vue'
import CountdownLabel from '../components/CountdownLabel.vue'
import HandCards from '../components/HandCards.vue'
import OpponentSeat from '../components/OpponentSeat.vue'
import { parseCsvNumberList } from '../net/kv'
import { useAuthStore } from '../stores/auth'
import { useRoomStore } from '../stores/room'
import { useSessionStore } from '../stores/session'
import { useSettlementStore } from '../stores/settlement'

const router = useRouter()
const authStore = useAuthStore()
const roomStore = useRoomStore()
const sessionStore = useSessionStore()
const settlementStore = useSettlementStore()

const selectedIndexes = ref<number[]>([])
const actionError = ref('')

const players = computed(() => roomStore.base.players)
const onlineFlags = computed(() => parseCsvNumberList(roomStore.base.players_online_bitmap))
const trusteePlayers = computed(() => new Set(parseCsvNumberList(roomStore.base.trustee_players)))
const playerCardCountMap = computed(() => {
  const out = new Map<number, number>()
  for (const item of roomStore.v2?.player_card_counts || []) {
    out.set(item.playerId, item.cardCount)
  }
  return out
})
const selectedCards = computed(() =>
  selectedIndexes.value
    .sort((a, b) => a - b)
    .map((index) => roomStore.privateHand.cards[index])
    .filter((value) => value !== undefined),
)

watch(
  () => settlementStore.latest,
  async (latest) => {
    if (latest) {
      await router.push('/settlement')
    }
  },
)

watch(
  () => roomStore.base.room_id,
  async (roomId) => {
    if (roomId <= 0 && authStore.loggedIn) {
      await router.push('/lobby')
    }
  },
  { immediate: true },
)

watch(
  () => authStore.loggedIn,
  async (loggedIn) => {
    if (!loggedIn) {
      await router.replace('/login')
    }
  },
  { immediate: true },
)

function toggleCard(card: number, index: number) {
  void card
  const cloned = [...selectedIndexes.value]
  const found = cloned.indexOf(index)
  if (found >= 0) {
    cloned.splice(found, 1)
  } else {
    cloned.push(index)
  }
  selectedIndexes.value = cloned
}

async function doCallScore(score: number) {
  actionError.value = ''
  try {
    await sessionStore.callScore(score)
  } catch (error) {
    actionError.value = error instanceof Error ? error.message : '叫分失败'
  }
}

async function doRob(rob: boolean) {
  actionError.value = ''
  try {
    await sessionStore.robLandlord(rob)
  } catch (error) {
    actionError.value = error instanceof Error ? error.message : '抢地主失败'
  }
}

async function doPlay() {
  actionError.value = ''
  if (selectedCards.value.length === 0) {
    actionError.value = '请先选择要出的牌'
    return
  }
  try {
    await sessionStore.playCards(selectedCards.value)
    selectedIndexes.value = []
  } catch (error) {
    actionError.value = error instanceof Error ? error.message : '出牌失败'
  }
}

async function doPass() {
  actionError.value = ''
  try {
    await sessionStore.passTurn()
    selectedIndexes.value = []
  } catch (error) {
    actionError.value = error instanceof Error ? error.message : '过牌失败'
  }
}
</script>

<template>
  <div class="page">
    <h2>房间 {{ roomStore.base.room_id || '-' }}</h2>
    <div class="room-meta">
      <div>房间状态：{{ roomStore.base.room_state }}</div>
      <div>当前操作：{{ roomStore.base.current_operator_player_id }}</div>
      <div>地主：{{ roomStore.base.landlord_player_id }}</div>
      <div>倍数(base_coin)：{{ roomStore.base.base_coin }}</div>
      <div>快照版本：{{ roomStore.base.snapshot_version }}</div>
      <CountdownLabel :deadline-ms="roomStore.v2?.action_deadline_ms || roomStore.base.nearest_offline_deadline_ms" label="倒计时" />
    </div>

    <div class="seats">
      <OpponentSeat
        v-for="(playerId, index) in players"
        :key="playerId || index"
        :player-id="playerId"
        :current-operator-player-id="roomStore.base.current_operator_player_id"
        :landlord-player-id="roomStore.base.landlord_player_id"
        :online="(onlineFlags[index] || 0) === 1"
        :trustee="trusteePlayers.has(playerId)"
        :card-count="playerCardCountMap.get(playerId) || 0"
      />
    </div>

    <div class="last-play">
      <span>上一手玩家：{{ roomStore.v2?.last_play_player_id || 0 }}</span>
      <span>上一手牌：{{ (roomStore.v2?.last_play_cards || []).join(',') || '-' }}</span>
      <span>底牌：{{ (roomStore.v2?.landlord_bottom_cards || []).join(',') || '-' }}</span>
    </div>

    <h3>我的手牌 ({{ roomStore.privateHand.cardCount }})</h3>
    <HandCards :cards="roomStore.privateHand.cards" :selected-indexes="selectedIndexes" @toggle="toggleCard" />
    <p class="selected">已选：{{ selectedCards.join(',') || '-' }}</p>

    <ActionBar :loading="sessionStore.busy" @call-score="doCallScore" @rob="doRob" @play="doPlay" @pass="doPass" />

    <p v-if="roomStore.reconnectNoticePlayerId > 0" class="notice">
      玩家 {{ roomStore.reconnectNoticePlayerId }} 已重连
    </p>
    <p v-if="actionError" class="error">{{ actionError }}</p>
    <p v-if="sessionStore.error" class="error">{{ sessionStore.error }}</p>
  </div>
</template>

<style scoped>
.page {
  width: min(980px, 100%);
  margin: 24px auto;
  padding: 16px;
  border: 1px solid #ddd7ec;
  border-radius: 12px;
  background: #fff;
}

.room-meta {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 8px;
  margin-bottom: 12px;
}

.seats {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 8px;
  margin-bottom: 12px;
}

.last-play {
  display: flex;
  flex-wrap: wrap;
  gap: 12px;
  margin-bottom: 12px;
  color: #635d74;
}

.selected {
  color: #4b4660;
}

.notice {
  color: #175f38;
}

.error {
  color: #b02020;
}

@media (max-width: 900px) {
  .room-meta {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }

  .seats {
    grid-template-columns: 1fr;
  }
}
</style>
