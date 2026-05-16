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
    <!-- Top Bar with Info -->
    <div class="table-header panel">
      <div class="header-left">
        <div class="room-id">ROOM #{{ roomStore.base.room_id || '---' }}</div>
        <div class="state-badge">{{ roomStore.base.room_state || 'WAITING' }}</div>
      </div>
      <div class="header-center">
        <div class="multiplier">
          <span>BASE COIN</span>
          <span class="value">{{ roomStore.base.base_coin || 0 }} x</span>
        </div>
      </div>
      <div class="header-right">
        <CountdownLabel 
          :deadline-ms="roomStore.v2?.action_deadline_ms || roomStore.base.nearest_offline_deadline_ms" 
          label="TIME" 
        />
      </div>
    </div>

    <!-- Opponents Section -->
    <div class="opponents-area">
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

    <!-- Center Play Area (Last played cards, bottom cards) -->
    <div class="center-table panel">
      <div class="table-section">
        <span class="section-label">LAST PLAY (P{{ roomStore.v2?.last_play_player_id || '-' }})</span>
        <div class="card-list">
          <span v-if="!(roomStore.v2?.last_play_cards?.length)">NO CARDS</span>
          <span v-for="card in roomStore.v2?.last_play_cards" :key="card" class="mini-card">{{ card }}</span>
        </div>
      </div>
      
      <div class="divider"></div>
      
      <div class="table-section">
        <span class="section-label">BOTTOM CARDS</span>
        <div class="card-list">
          <span v-if="!(roomStore.v2?.landlord_bottom_cards?.length)">???</span>
          <span v-for="card in roomStore.v2?.landlord_bottom_cards" :key="card" class="mini-card bottom-card">{{ card }}</span>
        </div>
      </div>
    </div>

    <!-- My Hand Area -->
    <div class="my-hand-area">
      <div class="hand-header">
        <div class="my-turn-indicator" v-if="roomStore.base.current_operator_player_id === authStore.playerId">
          👉 YOUR TURN! 👈
        </div>
        <div class="card-count">HAND CARDS: <span>{{ roomStore.privateHand.cardCount }}</span></div>
      </div>
      
      <HandCards :cards="roomStore.privateHand.cards" :selected-indexes="selectedIndexes" @toggle="toggleCard" />
    </div>

    <ActionBar :loading="sessionStore.busy" @call-score="doCallScore" @rob="doRob" @play="doPlay" @pass="doPass" />

    <div class="notifications">
      <div v-if="roomStore.reconnectNoticePlayerId > 0" class="toast-notice success">
        ♻️ PLAYER {{ roomStore.reconnectNoticePlayerId }} RECONNECTED
      </div>
      <div v-if="actionError" class="toast-notice error">
        ⚠️ {{ actionError }}
      </div>
      <div v-if="sessionStore.error" class="toast-notice error">
        🛑 {{ sessionStore.error }}
      </div>
    </div>
  </div>
</template>

<style scoped>
.page {
  width: min(1000px, 100%);
  margin: 16px auto;
  display: flex;
  flex-direction: column;
  gap: 16px;
}

/* Header */
.table-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px 24px;
}

.header-left, .header-right {
  display: flex;
  align-items: center;
  gap: 12px;
}

.room-id {
  font-size: 18px;
  font-weight: 700;
  color: var(--color-accent-gold);
}

.state-badge {
  background: rgba(255,255,255,0.1);
  padding: 4px 10px;
  border-radius: 8px;
  font-size: 12px;
  font-weight: 800;
  letter-spacing: 1px;
}

.multiplier {
  display: flex;
  flex-direction: column;
  align-items: center;
  background: rgba(0,0,0,0.5);
  padding: 4px 16px;
  border-radius: 12px;
  border: 1px solid var(--color-accent-gold);
  box-shadow: 0 0 10px rgba(255, 204, 0, 0.2);
}

.multiplier span:first-child {
  font-size: 10px;
  color: var(--color-accent-gold);
}

.multiplier .value {
  font-size: 20px;
  font-weight: 800;
  color: #fff;
}

/* Opponents */
.opponents-area {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 16px;
}

/* Center Table */
.center-table {
  display: flex;
  justify-content: space-around;
  padding: 24px;
  background: rgba(0,0,0,0.3);
  border-style: dashed;
}

.table-section {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 12px;
  flex: 1;
}

.section-label {
  font-size: 12px;
  font-weight: 800;
  color: var(--color-text-secondary);
  letter-spacing: 1px;
  background: rgba(0,0,0,0.5);
  padding: 4px 12px;
  border-radius: 12px;
}

.card-list {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
  justify-content: center;
  min-height: 48px;
  align-items: center;
}

.mini-card {
  background: #fff;
  color: #000;
  font-weight: 800;
  padding: 4px 8px;
  border-radius: 4px;
  border: 2px solid #ccc;
  box-shadow: 0 2px 4px rgba(0,0,0,0.3);
}

.bottom-card {
  border-color: var(--color-accent-gold);
}

.divider {
  width: 2px;
  background: rgba(255,255,255,0.1);
  border-radius: 1px;
}

/* My Hand Area */
.my-hand-area {
  margin-top: 24px;
  position: relative;
}

.hand-header {
  display: flex;
  justify-content: space-between;
  align-items: flex-end;
  padding: 0 16px;
  margin-bottom: -10px;
  z-index: 10;
  position: relative;
}

.my-turn-indicator {
  font-size: 20px;
  font-weight: 800;
  color: var(--color-accent-gold);
  background: rgba(0,0,0,0.7);
  padding: 4px 16px;
  border-radius: 20px;
  border: 2px solid var(--color-accent-gold);
  animation: bounce 0.5s infinite alternate;
}

@keyframes bounce {
  from { transform: translateY(0); }
  to { transform: translateY(-8px); }
}

.card-count {
  font-size: 14px;
  font-weight: 800;
  color: var(--color-text-secondary);
  background: rgba(0,0,0,0.5);
  padding: 4px 12px;
  border-radius: 12px;
}

.card-count span {
  color: #fff;
  font-size: 18px;
}

/* Notifications */
.notifications {
  position: fixed;
  bottom: 24px;
  left: 50%;
  transform: translateX(-50%);
  display: flex;
  flex-direction: column;
  gap: 8px;
  z-index: 1000;
}

.toast-notice {
  padding: 12px 24px;
  border-radius: 12px;
  font-weight: 700;
  font-size: 14px;
  box-shadow: 0 4px 12px rgba(0,0,0,0.5);
  animation: slide-up 0.3s ease-out;
}

.toast-notice.success {
  background: var(--color-accent-green);
  color: #0b4a24;
}

.toast-notice.error {
  background: var(--color-accent-red);
  color: #fff;
}

@keyframes slide-up {
  from { transform: translateY(20px); opacity: 0; }
  to { transform: translateY(0); opacity: 1; }
}

@media (max-width: 768px) {
  .table-header {
    flex-wrap: wrap;
    gap: 12px;
    justify-content: center;
  }
  .opponents-area {
    grid-template-columns: 1fr;
  }
  .my-turn-indicator {
    font-size: 14px;
  }
}
</style>
