<script setup lang="ts">
import { computed, watch } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '../stores/auth'
import { useLobbyStore } from '../stores/lobby'
import { useSessionStore } from '../stores/session'

const router = useRouter()
const authStore = useAuthStore()
const lobbyStore = useLobbyStore()
const sessionStore = useSessionStore()

const canStart = computed(() => authStore.loggedIn && !lobbyStore.matching)

watch(
  () => lobbyStore.matchedRoomId,
  async (roomId) => {
    if (roomId > 0) {
      await router.push('/room')
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

async function startMatch() {
  try {
    await sessionStore.startMatch()
  } catch (error) {
    sessionStore.error = error instanceof Error ? error.message : '匹配失败'
  }
}

async function cancelMatch() {
  try {
    await sessionStore.cancelMatch()
  } catch (error) {
    sessionStore.error = error instanceof Error ? error.message : '取消失败'
  }
}
</script>

<template>
  <div class="page panel lobby-panel">
    <div class="lobby-header">
      <h2>游戏大厅 <span>GAME LOBBY</span></h2>
    </div>
    
    <div class="player-card panel">
      <div class="avatar-placeholder">👾</div>
      <div class="player-details">
        <div class="name">{{ authStore.nickname || `Player_${authStore.playerId}` }}</div>
        <div class="id">ID: {{ authStore.playerId }}</div>
      </div>
      <div class="stats">
        <div class="stat-item">
          <div class="label">COINS</div>
          <div class="value gold">🪙 {{ authStore.coin }}</div>
        </div>
        <div class="stat-item">
          <div class="label">TOKEN</div>
          <div class="value">⏱️ {{ authStore.tokenSecondsRemaining }}s</div>
        </div>
      </div>
    </div>
    
    <div class="match-section">
      <div v-if="lobbyStore.matching" class="matching-status">
        <div class="spinner"></div>
        <div class="status-text">
          <h3>SEARCHING FOR PLAYERS...</h3>
          <p>请等待服务端推送 2005/2006</p>
        </div>
      </div>
      
      <div class="actions">
        <button v-if="!lobbyStore.matching" type="button" class="btn-chunky btn-gold match-btn" :disabled="!canStart || sessionStore.busy" @click="startMatch">
          ▶️ 开始匹配 MATCH (MODE 3)
        </button>
        <button v-else type="button" class="btn-chunky btn-red match-btn" :disabled="sessionStore.busy" @click="cancelMatch">
          ⏹️ 取消匹配 CANCEL
        </button>
      </div>
      
      <p v-if="lobbyStore.timeoutNotified" class="warn-msg">⚠️ 匹配超时，可重试 TIMEOUT</p>
      <p v-if="sessionStore.error" class="error-msg">⛔ {{ sessionStore.error }}</p>
    </div>
  </div>
</template>

<style scoped>
.page {
  width: min(700px, 100%);
  margin: 40px auto;
  padding: 32px;
  display: flex;
  flex-direction: column;
  gap: 24px;
}

.lobby-header h2 {
  color: var(--color-text-primary);
  font-size: 28px;
  display: flex;
  align-items: baseline;
  gap: 12px;
  border-bottom: 2px solid rgba(255,255,255,0.1);
  padding-bottom: 16px;
}

.lobby-header span {
  font-size: 16px;
  color: var(--color-accent-gold);
  letter-spacing: 2px;
}

.player-card {
  display: flex;
  align-items: center;
  gap: 20px;
  padding: 20px;
  background: rgba(0,0,0,0.5);
  border-color: rgba(255,255,255,0.05);
}

.avatar-placeholder {
  font-size: 48px;
  width: 80px;
  height: 80px;
  display: flex;
  align-items: center;
  justify-content: center;
  background: #2a2245;
  border: 4px solid var(--color-panel-border);
  border-radius: 16px;
  box-shadow: inset 0 4px 8px rgba(0,0,0,0.5);
}

.player-details {
  flex: 1;
}

.player-details .name {
  font-size: 24px;
  font-weight: 700;
  color: #fff;
  margin-bottom: 4px;
}

.player-details .id {
  font-size: 14px;
  color: var(--color-text-secondary);
  background: rgba(255,255,255,0.1);
  display: inline-block;
  padding: 2px 8px;
  border-radius: 6px;
}

.stats {
  display: flex;
  gap: 24px;
}

.stat-item {
  display: flex;
  flex-direction: column;
  align-items: flex-end;
}

.stat-item .label {
  font-size: 12px;
  color: var(--color-text-secondary);
  font-weight: 600;
  letter-spacing: 1px;
}

.stat-item .value {
  font-size: 20px;
  font-weight: 700;
}

.stat-item .value.gold {
  color: var(--color-accent-gold);
  text-shadow: 0 2px 4px rgba(0,0,0,0.5);
}

.match-section {
  display: flex;
  flex-direction: column;
  gap: 16px;
  margin-top: 16px;
}

.matching-status {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 20px;
  background: rgba(59, 130, 246, 0.15);
  border: 2px dashed rgba(59, 130, 246, 0.4);
  border-radius: 16px;
  padding: 24px;
  animation: pulse-bg 2s infinite alternate;
}

.spinner {
  width: 40px;
  height: 40px;
  border: 6px solid rgba(255,255,255,0.2);
  border-top-color: var(--color-accent-blue);
  border-radius: 50%;
  animation: spin 1s linear infinite;
}

.status-text h3 {
  color: var(--color-accent-blue);
  margin: 0 0 4px 0;
  font-size: 18px;
}

.status-text p {
  margin: 0;
  color: var(--color-text-secondary);
  font-size: 13px;
}

.match-btn {
  width: 100%;
  padding: 20px;
  font-size: 22px;
  letter-spacing: 2px;
}

.warn-msg {
  color: var(--color-accent-gold);
  background: rgba(255, 204, 0, 0.1);
  padding: 12px;
  border-radius: 8px;
  text-align: center;
  font-weight: 600;
}

.error-msg {
  color: #ff8c96;
  background: rgba(255, 71, 87, 0.15);
  padding: 12px;
  border-radius: 8px;
  text-align: center;
  font-weight: 600;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}

@keyframes pulse-bg {
  from { background: rgba(59, 130, 246, 0.05); }
  to { background: rgba(59, 130, 246, 0.15); }
}

@media (max-width: 600px) {
  .player-card {
    flex-direction: column;
    text-align: center;
  }
  .stats {
    width: 100%;
    justify-content: center;
    margin-top: 16px;
    border-top: 1px solid rgba(255,255,255,0.1);
    padding-top: 16px;
  }
  .stat-item {
    align-items: center;
  }
}
</style>
